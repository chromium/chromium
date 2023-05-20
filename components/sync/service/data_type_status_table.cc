// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/data_type_status_table.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/service/data_type_manager.h"

namespace syncer {

namespace {

ModelTypeSet GetTypesFromErrorMap(
    const DataTypeStatusTable::TypeErrorMap& errors) {
  ModelTypeSet result;
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

void DataTypeStatusTable::UpdateFailedDataTypes(const TypeErrorMap& errors) {
  DVLOG(1) << "Setting " << errors.size() << " new failed types.";

  for (const auto& [model_type, error] : errors) {
    UpdateFailedDataType(model_type, error);
  }
}

bool DataTypeStatusTable::UpdateFailedDataType(ModelType type,
                                               const SyncError& error) {
  switch (error.error_type()) {
    case SyncError::UNSET:
      NOTREACHED();
      break;
    case SyncError::DATATYPE_ERROR:
      return data_type_errors_.emplace(type, error).second;
    case SyncError::DATATYPE_POLICY_ERROR:
      return data_type_policy_errors_.emplace(type, error).second;
    case SyncError::CRYPTO_ERROR:
      return crypto_errors_.emplace(type, error).second;
    case SyncError::UNREADY_ERROR:
      return unready_errors_.emplace(type, error).second;
  }
  NOTREACHED();
  return false;
}

void DataTypeStatusTable::Reset() {
  DVLOG(1) << "Resetting data type errors.";
  data_type_errors_.clear();
  data_type_policy_errors_.clear();
  crypto_errors_.clear();
  unready_errors_.clear();
}

void DataTypeStatusTable::ResetCryptoErrors() {
  crypto_errors_.clear();
}

bool DataTypeStatusTable::ResetDataTypePolicyErrorFor(ModelType type) {
  return data_type_policy_errors_.erase(type) > 0;
}

bool DataTypeStatusTable::ResetUnreadyErrorFor(ModelType type) {
  return unready_errors_.erase(type) > 0;
}

DataTypeStatusTable::TypeErrorMap DataTypeStatusTable::GetAllErrors() const {
  TypeErrorMap result;
  result.insert(data_type_errors_.begin(), data_type_errors_.end());
  result.insert(data_type_policy_errors_.begin(),
                data_type_policy_errors_.end());
  result.insert(crypto_errors_.begin(), crypto_errors_.end());
  result.insert(unready_errors_.begin(), unready_errors_.end());
  return result;
}

ModelTypeSet DataTypeStatusTable::GetFailedTypes() const {
  ModelTypeSet result = GetFatalErrorTypes();
  result.PutAll(GetCryptoErrorTypes());
  result.PutAll(GetUnreadyErrorTypes());
  return result;
}

ModelTypeSet DataTypeStatusTable::GetFatalErrorTypes() const {
  ModelTypeSet result;
  result.PutAll(GetTypesFromErrorMap(data_type_errors_));
  result.PutAll(GetTypesFromErrorMap(data_type_policy_errors_));
  return result;
}

ModelTypeSet DataTypeStatusTable::GetCryptoErrorTypes() const {
  ModelTypeSet result = GetTypesFromErrorMap(crypto_errors_);
  return result;
}

ModelTypeSet DataTypeStatusTable::GetUnreadyErrorTypes() const {
  ModelTypeSet result = GetTypesFromErrorMap(unready_errors_);
  return result;
}

}  // namespace syncer
