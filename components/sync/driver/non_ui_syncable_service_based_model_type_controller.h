// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_NON_UI_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_NON_UI_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace syncer {

class SyncableService;

// Controller responsible for integrating SyncableService implementations within
// a non-blocking datatype (USS), for datatypes living outside the UI thread.
// This requires interacting with the SyncableService in a model thread that is
// not the UI thread, including the construction and destruction of objects
// (most notably SyncableServiceBasedBridge) in the model thread as specified
// in the constructor.
class NonUiSyncableServiceBasedModelTypeController
    : public ModelTypeController {
 public:
  using SyncableServiceProvider =
      base::OnceCallback<base::WeakPtr<syncer::SyncableService>()>;

  // |syncable_service_provider| and |store_factory| will be run on the backend
  // sequence, i.e. |task_runner|.
  NonUiSyncableServiceBasedModelTypeController(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      SyncableServiceProvider syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<SyncableServiceBasedBridge::ModelCryptographer>
          cryptographer = nullptr);
  ~NonUiSyncableServiceBasedModelTypeController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonUiSyncableServiceBasedModelTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_NON_UI_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_
