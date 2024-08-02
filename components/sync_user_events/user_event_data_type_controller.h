// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {

class DataTypeControllerDelegate;
class SyncService;

class UserEventDataTypeController : public syncer::DataTypeController,
                                    public syncer::SyncServiceObserver {
 public:
  // |sync_service| must not be null and must outlive this object.
  UserEventDataTypeController(
      SyncService* sync_service,
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode,
      std::unique_ptr<DataTypeControllerDelegate> delegate_for_transport_mode);

  UserEventDataTypeController(const UserEventDataTypeController&) = delete;
  UserEventDataTypeController& operator=(const UserEventDataTypeController&) =
      delete;

  ~UserEventDataTypeController() override;

  // syncer::DataTypeController implementation.
  void Stop(SyncStopMetadataFate fate, StopCallback callback) override;
  PreconditionState GetPreconditionState() const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  const raw_ptr<SyncService> sync_service_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_USER_EVENT_DATA_TYPE_CONTROLLER_H_
