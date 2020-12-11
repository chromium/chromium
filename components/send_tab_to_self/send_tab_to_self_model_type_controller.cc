// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "components/send_tab_to_self/features.h"

namespace send_tab_to_self {

SendTabToSelfModelTypeController::SendTabToSelfModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : ModelTypeController(syncer::SEND_TAB_TO_SELF,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)) {
  DCHECK_EQ(base::FeatureList::IsEnabled(
                send_tab_to_self::kSendTabToSelfWhenSignedIn),
            ShouldRunInTransportOnlyMode());
}

SendTabToSelfModelTypeController::~SendTabToSelfModelTypeController() = default;

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

}  // namespace send_tab_to_self
