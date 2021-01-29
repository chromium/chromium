// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {

class ModelTypeControllerDelegate;
class SyncService;

class UserEventModelTypeController : public syncer::ModelTypeController,
                                     public syncer::SyncServiceObserver {
 public:
  // |sync_service| must not be null and must outlive this object.
  UserEventModelTypeController(
      SyncService* sync_service,
      std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode);
  ~UserEventModelTypeController() override;

  // syncer::DataTypeController implementation.
  void Stop(ShutdownReason shutdown_reason, StopCallback callback) override;
  PreconditionState GetPreconditionState() const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  SyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(UserEventModelTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_MODEL_TYPE_CONTROLLER_H_
