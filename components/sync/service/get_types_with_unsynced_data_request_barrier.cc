// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/get_types_with_unsynced_data_request_barrier.h"

#include <utility>

#include "base/logging.h"

namespace syncer {

GetTypesWithUnsyncedDataRequestBarrier::GetTypesWithUnsyncedDataRequestBarrier(
    DataTypeSet requested_types,
    base::OnceCallback<void(DataTypeSet)> callback)
    : awaiting_types_(requested_types), callback_(std::move(callback)) {
  CHECK(!awaiting_types_.empty());
}

GetTypesWithUnsyncedDataRequestBarrier::
    ~GetTypesWithUnsyncedDataRequestBarrier() {
  if (!awaiting_types_.empty()) {
    DLOG(WARNING) << "GetTypesWithUnsyncedDataRequestHelper deleted before "
                     "request was fulfilled. Missing types are: "
                  << DataTypeSetToDebugString(awaiting_types_);
  }
}

void GetTypesWithUnsyncedDataRequestBarrier::OnReceivedResultForType(
    const DataType type,
    bool has_unsynced_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_unsynced_data) {
    types_with_unsynced_data_.Put(type);
  }

  // Remember that this part of the request is satisfied.
  CHECK(awaiting_types_.Has(type));
  awaiting_types_.Remove(type);

  if (awaiting_types_.empty()) {
    std::move(callback_).Run(types_with_unsynced_data_);
  }
}

}  // namespace syncer
