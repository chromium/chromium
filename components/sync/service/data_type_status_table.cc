// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_status_table.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/service/data_type_manager.h"

namespace syncer {

namespace {

DataTypeSet GetTypesFromErrorMap(
    const DataTypeStatusTable::TypeErrorMap& errors) {
  DataTypeSet result;
  for (const auto& [type, error] : errors) {
    DCHECK(!result.Has(type));
    result.Put(type);
  }
  return result;
}

}  // namespace

DataTypeStatusTable::DataTypeStatusTable() = default;

DataTypeStatusTable::DataTypeStatusTable(const DataTypeStatusTable& other) =
    default;

DataTypeStatusTable::~DataTypeStatusTable() = default;

bool DataTypeStatusTable::UpdateFailedDataType(DataType type,
                                               const SyncError& error) {
  switch (error.error_type()) {
    case SyncError::MODEL_ERROR:
    case SyncError::CONFIGURATION_ERROR:
      return data_type_errors_.emplace(type, error).second;
    case SyncError::CRYPTO_ERROR:
      return crypto_errors_.emplace(type, error).second;
    case SyncError::PRECONDITION_ERROR_WITH_KEEP_DATA:
      return precondition_errors_with_keep_data_.emplace(type, error).second;
    case SyncError::PRECONDITION_ERROR_WITH_CLEAR_DATA:
      return precondition_errors_with_clear_data_.emplace(type, error).second;
  }
  NOTREACHED();
}

void DataTypeStatusTable::Reset() {
  DVLOG(1) << "Resetting data type errors.";
  data_type_errors_.clear();
  precondition_errors_with_clear_data_.clear();
  crypto_errors_.clear();
  precondition_errors_with_keep_data_.clear();
}

void DataTypeStatusTable::ResetCryptoErrors() {
  crypto_errors_.clear();
}

bool DataTypeStatusTable::ResetPreconditionErrorFor(DataType type) {
  // Avoid operator || below to make sure `type` is removed from both sets, for
  // the hypothetical case where it is present in both.
  return (precondition_errors_with_clear_data_.erase(type) +
          precondition_errors_with_keep_data_.erase(type)) > 0;
}

DataTypeStatusTable::TypeErrorMap DataTypeStatusTable::GetAllErrors() const {
  TypeErrorMap result;
  result.insert(data_type_errors_.begin(), data_type_errors_.end());
  result.insert(precondition_errors_with_clear_data_.begin(),
                precondition_errors_with_clear_data_.end());
  result.insert(crypto_errors_.begin(), crypto_errors_.end());
  result.insert(precondition_errors_with_keep_data_.begin(),
                precondition_errors_with_keep_data_.end());
  return result;
}

DataTypeSet DataTypeStatusTable::GetFailedTypes() const {
  DataTypeSet result = GetFatalErrorTypes();
  result.PutAll(GetCryptoErrorTypes());
  result.PutAll(GetTypesFromErrorMap(precondition_errors_with_keep_data_));
  return result;
}

DataTypeSet DataTypeStatusTable::GetFatalErrorTypes() const {
  DataTypeSet result;
  result.PutAll(GetTypesFromErrorMap(data_type_errors_));
  result.PutAll(GetTypesFromErrorMap(precondition_errors_with_clear_data_));
  return result;
}

DataTypeSet DataTypeStatusTable::GetCryptoErrorTypes() const {
  DataTypeSet result = GetTypesFromErrorMap(crypto_errors_);
  return result;
}

}  // namespace syncer
