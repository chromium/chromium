// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_

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

  // Get the interested data types.
  const ModelTypeSet& GetInterestedDataTypes() const;

  // Set interested data types. The first call of the method initializes this
  // object.
  void SetInterestedDataTypes(
      const ModelTypeSet& data_types,
      SyncInvalidationsService::InterestedDataTypesAppliedCallback callback);

  // Returns true if SetInterestedDataTypes() has been called at least once.
  // Before that this object is considered to be uninitialized.
  bool IsInitialized() const;

 private:
  InterestedDataTypesHandler* interested_data_types_handler_ = nullptr;

  bool initialized_ = false;

  ModelTypeSet data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_INTERESTED_DATA_TYPES_MANAGER_H_
