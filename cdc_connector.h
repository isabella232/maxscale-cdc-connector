/* Copyright (c) 2017, MariaDB Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */

#include <cstdint>
#include <string>
#include <tr1/memory>
#include <vector>
#include <map>
#include <algorithm>
#include <jansson.h>

namespace CDC
{

// The error strings returned by the getError library. These can be used to
// check for the most common errors (which right now is only the timeout).
static const std::string TIMEOUT = "Request timed out";

// The typedef for the Row type
class InternalRow;
typedef std::tr1::shared_ptr<InternalRow> Row;

typedef std::vector<std::string> ValueList;
typedef std::map<std::string, std::string> ValueMap;

// A class that represents a CDC connection
class Connection
{
public:
    /**
     * Create a new CDC connection
     *
     * @param address  The address of the MaxScale server
     * @param port     The port where the CDC service listens
     * @param user     Username for the service
     * @param password Password for the user
     * @param timeout  Network operation timeout, both for reads and writes
     */
    Connection(const std::string& address,
               uint16_t port,
               const std::string& user,
               const std::string& password,
               int timeout = 10);
    virtual ~Connection();

    /**
     * Connect to MaxScale and request a data stream for a table
     *
     * @param table The table to stream in `database.table` format
     * @param gtid The optional starting GTID position in `domain-server_id-sequence` format
     *
     * @return True if the connection was successfully created and the stream was successfully requested
     */
    bool connect(const std::string& table, const std::string& gtid = "");

    /**
     * Read one change event
     *
     * @return A Row of data or an empty Row on error. The empty row evaluates
     * to false. If the read timed out, string returned by getError is empty.
     *
     * @see InternalRow
     */
    Row read();

    /**
     * Explicitly close the connection
     *
     * The connection is closed in the destructor if it is still open when it is called
     */
    void close();

    /**
     * Get the JSON schema in string form
     *
     * @return A reference to the string form of theJSON schema
     */
    const std::string& schema() const
    {
        return m_schema;
    }

    /**
     * Get the latest error
     *
     * @return The latest error or an empty string if no errors have occurred
     */
    const std::string& error() const
    {
        return m_error;
    }

    /**
     * Get the types of the fields mapped by field name
     *
     * @return A map of field names mapped to the SQL type
     */
    ValueMap fields() const
    {
        ValueMap fields;

        for (size_t i = 0; i < m_keys.size(); i++)
        {
            fields[m_keys[i]] = m_types[i];
        }

        return fields;
    }

private:
    int m_fd;
    uint16_t m_port;
    std::string m_address;
    std::string m_user;
    std::string m_password;
    std::string m_error;
    std::string m_schema;
    ValueList m_keys;
    ValueList m_types;
    int m_timeout;

    bool do_auth();
    bool do_registration();
    bool read_row(std::string& dest);
    void process_schema(json_t* json);
    Row process_row(json_t*);

    // Lower-level functions
    int wait_for_event(short events);
    int nointr_read(void *dest, size_t size);
    int nointr_write(const void *src, size_t size);
};

// Internal representation of a row, used via the Row type
class InternalRow
{
public:

    /**
     * Get field count for the row
     *
     * @return Number of fields in row
     */
    size_t length() const
    {
        return m_values.size();
    }

    /**
     * Get the value of a field by index
     *
     * @param i The field index
     *
     * @return A reference to the internal value
     */
    const std::string& value(size_t i) const
    {
        return m_values[i];
    }

    /**
     * Get the value of a field by name
     *
     * @param i The field index to get
     *
     * @return A reference to the internal value
     */
    const std::string& value(const std::string& str) const
    {
        ValueList::const_iterator it = std::find(m_keys.begin(), m_keys.end(), str);
        return m_values[it - m_keys.begin()];
    }

    /**
     * Get the GTID of this row
     *
     * @return The GTID of the row in `domain-server_id-sequence` format
     */
    const std::string gtid() const
    {
        std::string s;
        s += value("domain");
        s += "-";
        s += value("server_id");
        s +=  "-";
        s += value("sequence");
        return s;
    }

    /**
     * Get field names by index
     *
     * @return Reference to field name
     */
    const std::string& key(size_t i) const
    {
        return m_keys[i];
    }

    /**
     * Get field types by index
     *
     * @return Reference to field type
     */
    const std::string& type(size_t i) const
    {
        return m_types[i];
    }

    ~InternalRow()
    {
    }

private:
    ValueList m_keys;
    ValueList m_types;
    ValueList m_values;

    // Not intended to be copied
    InternalRow(const InternalRow&);
    InternalRow& operator=(const InternalRow&);
    InternalRow();

    // Only a Connection should construct an InternalRow
    friend class Connection;

    InternalRow(const ValueList& keys,
                const ValueList& types,
                const ValueList& values):
        m_keys(keys),
        m_types(types),
        m_values(values)
    {
    }

};

}
