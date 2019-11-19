// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

class SyncableService;

// Controller responsible for integrating SyncableService implementations within
// a non-blocking datatype (USS), for datatypes living in the UI thread.
class SyncableServiceBasedModelTypeController : public ModelTypeController {
 public:
  // |syncable_service| may be null in tests.
  SyncableServiceBasedModelTypeController(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack);
  ~SyncableServiceBasedModelTypeController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncableServiceBasedModelTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_
