// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_data_type_controller.h"

#include <utility>

#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace sync_tab_context {

TabContextDataTypeController::TabContextDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service)
    : DataTypeController(type,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {
  CHECK(type == syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER ||
        type == syncer::ENCRYPTED_TAB_CONTEXT_ITEM)
      << syncer::DataTypeToDebugString(type);
  CHECK(sync_service_);
  sync_observation_.Observe(sync_service_);
}

TabContextDataTypeController::~TabContextDataTypeController() = default;

syncer::DataTypeController::PreconditionState
TabContextDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  DCHECK(CalledOnValidThread());

  if (sync_service_->GetUserSettings()->GetPassphraseType() !=
      syncer::PassphraseType::kTrustedVaultPassphrase) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndClearData;
  }

  // Dasher (enterprise) users are excluded.
  switch (context.account_managed_status) {
    case signin::AccountManagedStatusFinderOutcome::kConsumerGmail:
    case signin::AccountManagedStatusFinderOutcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kConsumerNotWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom:
      break;
    case signin::AccountManagedStatusFinderOutcome::kEnterprise:
      return syncer::DataTypeController::PreconditionState::
          kMustStopAndClearData;
    case signin::AccountManagedStatusFinderOutcome::kPending:
    case signin::AccountManagedStatusFinderOutcome::kError:
    case signin::AccountManagedStatusFinderOutcome::kTimeout:
      return syncer::DataTypeController::PreconditionState::
          kMustStopAndKeepData;
  }

  return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
}

void TabContextDataTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void TabContextDataTypeController::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

}  // namespace sync_tab_context
