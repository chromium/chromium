// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_data_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "components/send_tab_to_self/features.h"

namespace send_tab_to_self {

SendTabToSelfDataTypeController::SendTabToSelfDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode)
    : DataTypeController(syncer::SEND_TAB_TO_SELF,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)) {
  DCHECK(ShouldRunInTransportOnlyMode());
}

SendTabToSelfDataTypeController::~SendTabToSelfDataTypeController() = default;

void SendTabToSelfDataTypeController::Stop(syncer::SyncStopMetadataFate fate,
                                            StopCallback callback) {
  DCHECK(CalledOnValidThread());
  // Special case: We want to clear all data even when Sync is stopped
  // temporarily, regardless of incoming fate. This is also needed to make sure
  // the feature stops being offered to the user, because predicates like
  // IsUserSyncTypeActive() should return false upon stop.
  DataTypeController::Stop(syncer::SyncStopMetadataFate::CLEAR_METADATA,
                            std::move(callback));
}

}  // namespace send_tab_to_self
