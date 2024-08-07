// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_GET_TYPES_WITH_UNSYNCED_DATA_REQUEST_BARRIER_H_
#define COMPONENTS_SYNC_SERVICE_GET_TYPES_WITH_UNSYNCED_DATA_REQUEST_BARRIER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/sync/base/data_type.h"

namespace syncer {

// A helper class for SyncServiceImpl's implementation of
// GetTypesWithUnsyncedData. It accumulates the responses from each type's
// DataTypeController, and runs the supplied callback once they're all done.
class GetTypesWithUnsyncedDataRequestBarrier
    : public base::RefCounted<GetTypesWithUnsyncedDataRequestBarrier> {
 public:
  // Once `OnReceivedResultForType` has been called for each type in
  // `requested_types`, the `callback` will be run. `requested_types` must not
  // be empty.
  GetTypesWithUnsyncedDataRequestBarrier(
      DataTypeSet requested_types,
      base::OnceCallback<void(DataTypeSet)> callback);

  GetTypesWithUnsyncedDataRequestBarrier(
      const GetTypesWithUnsyncedDataRequestBarrier&) = delete;
  GetTypesWithUnsyncedDataRequestBarrier& operator=(
      const GetTypesWithUnsyncedDataRequestBarrier&) = delete;

  void OnReceivedResultForType(const DataType type, bool has_unsynced_data);

 private:
  friend class base::RefCounted<GetTypesWithUnsyncedDataRequestBarrier>;
  virtual ~GetTypesWithUnsyncedDataRequestBarrier();

  DataTypeSet awaiting_types_;
  DataTypeSet types_with_unsynced_data_;
  base::OnceCallback<void(DataTypeSet)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_GET_TYPES_WITH_UNSYNCED_DATA_REQUEST_BARRIER_H_
