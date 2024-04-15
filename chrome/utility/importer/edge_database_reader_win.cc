// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/edge_database_reader_win.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace {

// This is an arbitary size chosen for the database error message buffer.
const size_t kErrorMessageSize = 1024;
// This is the page size of the Edge data. It's unlikely to change.
const JET_API_PTR kEdgeDatabasePageSize = 8192;
// This is the code page value for a Unicode (UCS-2) column.
const unsigned short kJetUnicodeCodePage = 1200;

template <typename T>
bool ValidateAndConvertValueGeneric(const JET_COLTYP match_column_type,
                                    const JET_COLTYP column_type,
                                    const std::vector<uint8_t>& column_data,
                                    T* value) {
  if ((column_type == match_column_type) && (column_data.size() == sizeof(T))) {
    memcpy(value, &column_data[0], sizeof(T));
    return true;
  }
  return false;
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             bool* value) {
  if ((column_type == JET_coltypBit) && (column_data.size() == 1)) {
    *value = (column_data[0] & 1) == 1;
    return true;
  }
  return false;
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             std::u16string* value) {
  if ((column_type == JET_coltypLongText) &&
      ((column_data.size() % sizeof(char16_t)) == 0)) {
    std::u16string& value_ref = *value;
    size_t char_length = column_data.size() / sizeof(char16_t);
    value_ref.resize(char_length);
    memcpy(&value_ref[0], &column_data[0], column_data.size());
    // Remove any trailing NUL characters.
    while (char_length > 0) {
      if (value_ref[char_length - 1])
        break;
      char_length--;
    }
    value_ref.resize(char_length);
    return true;
  }
  return false;
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             GUID* value) {
  return ValidateAndConvertValueGeneric(JET_coltypGUID, column_type,
                                        column_data, value);
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             int32_t* value) {
  return ValidateAndConvertValueGeneric(JET_coltypLong, column_type,
                                        column_data, value);
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             int64_t* value) {
  return ValidateAndConvertValueGeneric(JET_coltypLongLong, column_type,
                                        column_data, value);
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             FILETIME* value) {
  return ValidateAndConvertValueGeneric(JET_coltypLongLong, column_type,
                                        column_data, value);
}

bool ValidateAndConvertValue(const JET_COLTYP column_type,
                             const std::vector<uint8_t>& column_data,
                             uint32_t* value) {
  return ValidateAndConvertValueGeneric(JET_coltypUnsignedLong, column_type,
                                        column_data, value);
}

}  // namespace

std::wstring EdgeErrorObject::GetErrorMessage() const {
  WCHAR error_message[kErrorMessageSize] = {};
  JET_API_PTR err = last_error_;
  JET_ERR result = JetGetSystemParameter(JET_instanceNil, JET_sesidNil,
                                         JET_paramErrorToString, &err,
                                         error_message, sizeof(error_message));
  if (result != JET_errSuccess)
    return L"";

  return error_message;
}

bool EdgeErrorObject::SetLastError(JET_ERR error) {
  last_error_ = error;
  return error == JET_errSuccess;
}

EdgeDatabaseTableEnumerator::EdgeDatabaseTableEnumerator(
    const std::wstring& table_name,
    JET_SESID session_id,
    JET_TABLEID table_id)
    : table_id_(table_id), table_name_(table_name), session_id_(session_id) {}

EdgeDatabaseTableEnumerator::~EdgeDatabaseTableEnumerator() {
  if (table_id_ != JET_tableidNil)
    JetCloseTable(session_id_, table_id_);
}

bool EdgeDatabaseTableEnumerator::Reset() {
  return SetLastError(JetMove(session_id_, table_id_, JET_MoveFirst, 0));
}

bool EdgeDatabaseTableEnumerator::Next() {
  return SetLastError(JetMove(session_id_, table_id_, JET_MoveNext, 0));
}

template <typename T>
bool EdgeDatabaseTableEnumerator::RetrieveColumn(
    const std::wstring& column_name,
    T* value) {
  const JET_COLUMNBASE& column_base = GetColumnByName(column_name);
  if (column_base.cbMax == 0) {
    SetLastError(JET_errColumnNotFound);
    return false;
  }
  if (column_base.coltyp == JET_coltypLongText &&
      column_base.cp != kJetUnicodeCodePage) {
    SetLastError(JET_errInvalidColumnType);
    return false;
  }
  std::vector<uint8_t> column_data(column_base.cbMax);
  unsigned long actual_size = 0;
  JET_ERR err = JetRetrieveColumn(session_id_, table_id_, column_base.columnid,
                                  &column_data[0], column_data.size(),
                                  &actual_size, 0, nullptr);
  SetLastError(err);
  if (err != JET_errSuccess && err != JET_wrnColumnNull) {
    return false;
  }

  if (err == JET_errSuccess) {
    column_data.resize(actual_size);
    if (!ValidateAndConvertValue(column_base.coltyp, column_data, value)) {
      SetLastError(JET_errInvalidColumnType);
      return false;
    }
  } else {
    *value = T();
  }

  return true;
}

// Explicitly instantiate implementations of RetrieveColumn for various types.
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          bool*);
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          FILETIME*);
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          GUID*);
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          int32_t*);
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          int64_t*);
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          std::u16string*);
template bool EdgeDatabaseTableEnumerator::RetrieveColumn(const std::wstring&,
                                                          uint32_t*);

const JET_COLUMNBASE& EdgeDatabaseTableEnumerator::GetColumnByName(
    const std::wstring& column_name) {
  auto found_col = columns_by_name_.find(column_name);
  if (found_col == columns_by_name_.end()) {
    JET_COLUMNBASE column_base = {};
    column_base.cbStruct = sizeof(JET_COLUMNBASE);
    if (!SetLastError(JetGetTableColumnInfo(
            session_id_, table_id_, column_name.c_str(), &column_base,
            sizeof(column_base), JET_ColInfoBase))) {
      // 0 indicates an invalid column.
      column_base.cbMax = 0;
    }
    columns_by_name_[column_name] = column_base;
    found_col = columns_by_name_.find(column_name);
  }
  return found_col->second;
}

EdgeDatabaseReader::~EdgeDatabaseReader() {
  // We don't need to collect other ID handles, terminating instance
  // is enough to shut the entire session down.
  if (instance_id_ != JET_instanceNil)
    JetTerm(instance_id_);
}

bool EdgeDatabaseReader::OpenDatabase(const base::FilePath& database_file) {
  if (IsOpen()) {
    SetLastError(JET_errOneDatabasePerSession);
    return false;
  }
  if (!SetLastError(JetSetSystemParameter(nullptr, JET_sesidNil,
                                          JET_paramDatabasePageSize,
                                          kEdgeDatabasePageSize, nullptr)))
    return false;
  if (!SetLastError(JetCreateInstance(&instance_id_, L"EdgeDataImporter")))
    return false;
  if (!log_folder_.empty()) {
    if (!SetLastError(JetSetSystemParameter(&instance_id_, JET_sesidNil,
                                            JET_paramLogFilePath, 0,
                                            log_folder_.value().c_str())))
      return false;
    // Set location of checkpoint file "edb.chk", which stores persistent state.
    if (!SetLastError(JetSetSystemParameter(&instance_id_, JET_sesidNil,
                                            JET_paramSystemPath, 0,
                                            log_folder_.value().c_str()))) {
      return false;
    }
  } else {
    if (!SetLastError(JetSetSystemParameter(&instance_id_, JET_sesidNil,
                                          JET_paramRecovery, 0, L"Off")))
      return false;
  }
  if (!SetLastError(JetInit(&instance_id_)))
    return false;
  if (!SetLastError(
          JetBeginSession(instance_id_, &session_id_, nullptr, nullptr)))
    return false;
  if (!SetLastError(JetAttachDatabase2(
          session_id_, database_file.value().c_str(), 0, JET_bitDbReadOnly)))
    return false;
  if (!SetLastError(JetOpenDatabase(session_id_, database_file.value().c_str(),
                                    nullptr, &db_id_, JET_bitDbReadOnly)))
    return false;
  return true;
}

std::unique_ptr<EdgeDatabaseTableEnumerator>
EdgeDatabaseReader::OpenTableEnumerator(const std::wstring& table_name) {
  JET_TABLEID table_id;

  if (!IsOpen()) {
    SetLastError(JET_errDatabaseNotFound);
    return nullptr;
  }

  if (!SetLastError(JetOpenTable(session_id_, db_id_, table_name.c_str(),
                                 nullptr, 0, JET_bitTableReadOnly, &table_id)))
    return nullptr;

  return std::make_unique<EdgeDatabaseTableEnumerator>(table_name, session_id_,
                                                       table_id);
}
