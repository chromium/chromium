// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_NON_UI_SYNCABLE_SERVICE_BASED_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_SERVICE_NON_UI_SYNCABLE_SERVICE_BASED_DATA_TYPE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/service/data_type_controller.h"

namespace syncer {

class SyncableService;

// Controller responsible for integrating legacy data type implementations
// (SyncableService) within the new sync architecture (USS), for types living
// outside the UI thread.
// This requires interacting with the SyncableService in a model thread that is
// not the UI thread, including the construction and destruction of objects
// (most notably SyncableServiceBasedBridge) in the model thread as specified
// in the constructor.
class NonUiSyncableServiceBasedDataTypeController : public DataTypeController {
 public:
  using SyncableServiceProvider =
      base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>;

  enum class DelegateMode {
    // The data type runs only in full-sync mode. This is deprecated; new data
    // types should not use this!
    kLegacyFullSyncModeOnly,
    // The data type runs in both full-sync mode and transport mode, and it
    // shares a single data model across both modes (i.e. the data type does not
    // distinguish between syncing users and signed-in non-syncing users).
    kTransportModeWithSingleModel
  };

  // `syncable_service_provider` and `store_factory` will be run on the backend
  // sequence, i.e. `task_runner`.
  // `delegate_mode` determines whether only a single delegate (for full-sync
  // mode) will be created, or two separate delegates for both full-sync and
  // transport mode.
  NonUiSyncableServiceBasedDataTypeController(
      DataType type,
      OnceDataTypeStoreFactory store_factory,
      SyncableServiceProvider syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      DelegateMode delegate_mode);

  NonUiSyncableServiceBasedDataTypeController(
      const NonUiSyncableServiceBasedDataTypeController&) = delete;
  NonUiSyncableServiceBasedDataTypeController& operator=(
      const NonUiSyncableServiceBasedDataTypeController&) = delete;

  ~NonUiSyncableServiceBasedDataTypeController() override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_NON_UI_SYNCABLE_SERVICE_BASED_DATA_TYPE_CONTROLLER_H_
