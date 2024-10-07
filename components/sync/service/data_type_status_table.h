// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_STATUS_TABLE_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_STATUS_TABLE_H_

#include <map>

#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_error.h"

namespace syncer {

// Class to keep track of data types that have encountered an error during sync.
class DataTypeStatusTable {
 public:
  using TypeErrorMap = std::map<DataType, SyncError>;

  DataTypeStatusTable();
  DataTypeStatusTable(const DataTypeStatusTable& other);
  ~DataTypeStatusTable();

  // Copy and assign welcome.

  // Update an individual failed datatype. The type will be added to its
  // corresponding error map based on |error.error_type()|. Returns true if
  // there was an actual change.
  bool UpdateFailedDataType(DataType type, const SyncError& error);

  // Resets the current set of data type errors.
  void Reset();

  // Resets the set of types with cryptographer errors.
  void ResetCryptoErrors();

  // Resets the set of types with precondition errors (regardless of clear vs
  // keep data). Returns true if the type was removed from the error set, false
  // if the type did not have a precondition error to begin with.
  bool ResetPreconditionErrorFor(DataType type);

  // Returns a list of all the errors this class has recorded.
  TypeErrorMap GetAllErrors() const;

  // Returns all types with failure errors. This includes, fatal, crypto, and
  // unready types.
  DataTypeSet GetFailedTypes() const;

  // Returns the types that are failing due to model errors, configuration
  // errors or policy errors.
  DataTypeSet GetFatalErrorTypes() const;

  // Returns the types that are failing due to cryptographer errors.
  DataTypeSet GetCryptoErrorTypes() const;

 private:
  // List of data types that failed due to runtime errors and should be
  // disabled.
  TypeErrorMap data_type_errors_;
  TypeErrorMap precondition_errors_with_clear_data_;

  // List of data types that failed due to the cryptographer not being ready.
  TypeErrorMap crypto_errors_;

  // List of data types that could not start due to not being ready.
  TypeErrorMap precondition_errors_with_keep_data_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_STATUS_TABLE_H_
