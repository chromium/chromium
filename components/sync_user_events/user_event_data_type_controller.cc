// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_user_events/user_event_data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {

UserEventDataTypeController::UserEventDataTypeController(
    SyncService* sync_service,
    std::unique_ptr<DataTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<DataTypeControllerDelegate> delegate_for_transport_mode)
    : DataTypeController(syncer::USER_EVENTS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {
  DCHECK(sync_service_);
  sync_service_->AddObserver(this);
}

UserEventDataTypeController::~UserEventDataTypeController() {
  sync_service_->RemoveObserver(this);
}

void UserEventDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                        StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // Special case: For USER_EVENT, we want to clear all data even when Sync is
  // stopped temporarily.
  DataTypeController::Stop(syncer::SyncStopMetadataFate::CLEAR_METADATA,
                            std::move(callback));
}

DataTypeController::PreconditionState
UserEventDataTypeController::GetPreconditionState() const {
  return sync_service_->GetUserSettings()->IsUsingExplicitPassphrase()
             ? PreconditionState::kMustStopAndClearData
             : PreconditionState::kPreconditionsMet;
}

void UserEventDataTypeController::OnStateChanged(syncer::SyncService* sync) {
  sync->DataTypePreconditionChanged(type());
}

}  // namespace syncer
