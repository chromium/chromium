// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_container_data_type_controller.h"

#include <utility>

#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace sync_tab_context {

TabContextContainerDataTypeController::TabContextContainerDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service)
    : DataTypeController(syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {
  CHECK(sync_service_);
  sync_observation_.Observe(sync_service_);
}

TabContextContainerDataTypeController::
    ~TabContextContainerDataTypeController() = default;

syncer::DataTypeController::PreconditionState
TabContextContainerDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  DCHECK(CalledOnValidThread());

  if (sync_service_->GetUserSettings()->GetPassphraseType() !=
      syncer::PassphraseType::kTrustedVaultPassphrase) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndClearData;
  }

  return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void TabContextContainerDataTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void TabContextContainerDataTypeController::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_observation_.Reset();
}

}  // namespace sync_tab_context
