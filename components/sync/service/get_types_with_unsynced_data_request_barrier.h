// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_GET_TYPES_WITH_UNSYNCED_DATA_REQUEST_BARRIER_H_
#define COMPONENTS_SYNC_SERVICE_GET_TYPES_WITH_UNSYNCED_DATA_REQUEST_BARRIER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"

namespace syncer {

// A helper class for SyncServiceImpl's implementation of
// GetTypesWithUnsyncedData. It accumulates the responses from each type's
// ModelTypeController, and runs the supplied callback once they're all done.
class GetTypesWithUnsyncedDataRequestBarrier
    : public base::RefCounted<GetTypesWithUnsyncedDataRequestBarrier> {
 public:
  // Once `OnReceivedResultForType` has been called for each type in
  // `requested_types`, the `callback` will be run. `requested_types` must not
  // be empty.
  GetTypesWithUnsyncedDataRequestBarrier(
      ModelTypeSet requested_types,
      base::OnceCallback<void(ModelTypeSet)> callback);

  GetTypesWithUnsyncedDataRequestBarrier(
      const GetTypesWithUnsyncedDataRequestBarrier&) = delete;
  GetTypesWithUnsyncedDataRequestBarrier& operator=(
      const GetTypesWithUnsyncedDataRequestBarrier&) = delete;

  void OnReceivedResultForType(const ModelType type, bool has_unsynced_data);

 private:
  friend class base::RefCounted<GetTypesWithUnsyncedDataRequestBarrier>;
  virtual ~GetTypesWithUnsyncedDataRequestBarrier();

  ModelTypeSet awaiting_types_;
  ModelTypeSet types_with_unsynced_data_;
  base::OnceCallback<void(ModelTypeSet)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_GET_TYPES_WITH_UNSYNCED_DATA_REQUEST_BARRIER_H_
