// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNCABLE_SERVICE_BASED_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SERVICE_SYNCABLE_SERVICE_BASED_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/service/data_type_controller.h"

namespace syncer {

class SyncableService;

// Controller responsible for integrating legacy data type implementations
// (SyncableService) within the new sync architecture (USS), for types living on
// the UI thread.
class SyncableServiceBasedDataTypeController : public DataTypeController {
 public:
  enum class DelegateMode {
    // The data type runs only in full-sync mode. This is deprecated; new data
    // types should not use this!
    kLegacyFullSyncModeOnly,
    // The data type runs in both full-sync mode and transport mode, and it
    // shares a single data model across both modes (i.e. the data type does not
    // distinguish between syncing users and signed-in non-syncing users).
    kTransportModeWithSingleModel
  };

  // `syncable_service` may be null in some cases, e.g. when the underlying
  // service failed to initialize, and in tests.
  // `delegate_mode` determines whether only a single delegate (for full-sync
  // mode) will be created, or two separate delegates for both full-sync and
  // transport mode.
  SyncableServiceBasedDataTypeController(
      DataType type,
      OnceDataTypeStoreFactory store_factory,
      base::WeakPtr<SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      DelegateMode delegate_mode);

  SyncableServiceBasedDataTypeController(
      const SyncableServiceBasedDataTypeController&) = delete;
  SyncableServiceBasedDataTypeController& operator=(
      const SyncableServiceBasedDataTypeController&) = delete;

  ~SyncableServiceBasedDataTypeController() override;

 private:
  // Delegate owned by this instance; delegate instances passed to the base
  // class forward their calls to `delegate_`.
  std::unique_ptr<DataTypeControllerDelegate> delegate_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNCABLE_SERVICE_BASED_DATA_TYPE_CONTROLLER_H_
