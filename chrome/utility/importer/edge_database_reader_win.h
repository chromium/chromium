// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_EDGE_DATABASE_READER_WIN_H_
#define CHROME_UTILITY_IMPORTER_EDGE_DATABASE_READER_WIN_H_

#define JET_UNICODE
#include <esent.h>
#undef JET_UNICODE

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"

class EdgeErrorObject {
 public:
  EdgeErrorObject() : last_error_(JET_errSuccess) {}

  EdgeErrorObject(const EdgeErrorObject&) = delete;
  EdgeErrorObject& operator=(const EdgeErrorObject&) = delete;

  // Get the last error converted to a descriptive string.
  std::wstring GetErrorMessage() const;
  // Get the last error value.
  JET_ERR last_error() const { return last_error_; }

 protected:
  // This function returns true if the passed error parameter is equal
  // to JET_errSuccess
  bool SetLastError(JET_ERR error);

 private:
  JET_ERR last_error_;
};

class EdgeDatabaseTableEnumerator : public EdgeErrorObject {
 public:
  EdgeDatabaseTableEnumerator(const std::wstring& table_name,
                              JET_SESID session_id,
                              JET_TABLEID table_id);

  EdgeDatabaseTableEnumerator(const EdgeDatabaseTableEnumerator&) = delete;
  EdgeDatabaseTableEnumerator& operator=(const EdgeDatabaseTableEnumerator&) =
      delete;

  ~EdgeDatabaseTableEnumerator();

  const std::wstring& table_name() { return table_name_; }

  // Reset the enumerator to the start of the table. Returns true if successful.
  bool Reset();
  // Move to the next row in the table. Returns false on error or no more rows.
  bool Next();

  // Retrieve a column's data value. If a NULL is encountered in the column the
  // default value for the template type is placed in |value|.
  template <typename T>
  bool RetrieveColumn(const std::wstring& column_name, T* value);

 private:
  const JET_COLUMNBASE& GetColumnByName(const std::wstring& column_name);

  std::map<const std::wstring, JET_COLUMNBASE> columns_by_name_;
  JET_TABLEID table_id_;
  std::wstring table_name_;
  JET_SESID session_id_;
};

class EdgeDatabaseReader : public EdgeErrorObject {
 public:
  EdgeDatabaseReader()
      : db_id_(JET_dbidNil),
        instance_id_(JET_instanceNil),
        session_id_(JET_sesidNil) {}

  EdgeDatabaseReader(const EdgeDatabaseReader&) = delete;
  EdgeDatabaseReader& operator=(const EdgeDatabaseReader&) = delete;

  ~EdgeDatabaseReader();

  // Open the database from a file path. Returns true on success.
  bool OpenDatabase(const base::FilePath& database_file);

  void set_log_folder(const base::FilePath& log_folder) {
    log_folder_ = log_folder;
  }

  // Open a row enumerator for a specified table. Returns a nullptr on error.
  std::unique_ptr<EdgeDatabaseTableEnumerator> OpenTableEnumerator(
      const std::wstring& table_name);

 private:
  bool IsOpen() { return instance_id_ != JET_instanceNil; }

  // This specifies the optional location of the folder where the ESE database
  // will write the log of a possible recovery from a corrupted database.
  // When specified, the folder must exist, or opening the database will fail.
  base::FilePath log_folder_;

  JET_DBID db_id_;
  JET_INSTANCE instance_id_;
  JET_SESID session_id_;
};

#endif  // CHROME_UTILITY_IMPORTER_EDGE_DATABASE_READER_WIN_H_
