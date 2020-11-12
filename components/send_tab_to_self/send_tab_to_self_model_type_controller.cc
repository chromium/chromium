// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "components/send_tab_to_self/features.h"
#include "components/sync/driver/sync_auth_util.h"
#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace send_tab_to_self {

SendTabToSelfModelTypeController::SendTabToSelfModelTypeController(
    syncer::SyncService* sync_service,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : ModelTypeController(syncer::SEND_TAB_TO_SELF,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {
  DCHECK_EQ(base::FeatureList::IsEnabled(
                send_tab_to_self::kSendTabToSelfWhenSignedIn),
            ShouldRunInTransportOnlyMode());
  // TODO(crbug.com/906995): Remove this observing mechanism once all sync
  // datatypes are stopped by ProfileSyncService, when sync is paused.
  sync_service_->AddObserver(this);
}

SendTabToSelfModelTypeController::~SendTabToSelfModelTypeController() {
  sync_service_->RemoveObserver(this);
}

void SendTabToSelfModelTypeController::Stop(
    syncer::ShutdownReason shutdown_reason,
    StopCallback callback) {
  DCHECK(CalledOnValidThread());
  switch (shutdown_reason) {
    case syncer::STOP_SYNC:
      // Special case: We want to clear all data even when Sync is stopped
      // temporarily. This is also needed to make sure the feature stops being
      // offered to the user, because predicates like IsUserSyncTypeActive()
      // should return false upon stop.
      shutdown_reason = syncer::DISABLE_SYNC;
      break;
    case syncer::DISABLE_SYNC:
    case syncer::BROWSER_SHUTDOWN:
      break;
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
}

syncer::DataTypeController::PreconditionState
SendTabToSelfModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return syncer::IsWebSignout(sync_service_->GetAuthError())
             ? PreconditionState::kMustStopAndClearData
             : PreconditionState::kPreconditionsMet;
}

void SendTabToSelfModelTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  // Most of these calls will be no-ops but SyncService handles that just fine.
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace send_tab_to_self
