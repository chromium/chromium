// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

namespace syncer {
class InterestedDataTypesHandler;

// Manages for which data types are invalidations sent to this device.
class InterestedDataTypesManager {
 public:
  InterestedDataTypesManager();
  ~InterestedDataTypesManager();
  InterestedDataTypesManager(const InterestedDataTypesManager&) = delete;
  InterestedDataTypesManager& operator=(const InterestedDataTypesManager&) =
      delete;

  // Set the interested data types change handler. |handler| can be nullptr to
  // unregister any existing handler. There can be at most one handler.
  void SetInterestedDataTypesHandler(InterestedDataTypesHandler* handler);

  // Get the interested data types. Returns nullopt if SetInterestedDataTypes()
  // has never been called.
  absl::optional<ModelTypeSet> GetInterestedDataTypes() const;

  // Set interested data types. The first call of the method initializes this
  // object.
  void SetInterestedDataTypes(const ModelTypeSet& data_types);

  void SetCommittedAdditionalInterestedDataTypesCallback(
      SyncInvalidationsService::InterestedDataTypesAppliedCallback callback);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<InterestedDataTypesHandler> interested_data_types_handler_ = nullptr;

  absl::optional<ModelTypeSet> data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_
