// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_data_type_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace history {

namespace {

std::unique_ptr<syncer::DataTypeControllerDelegate>
GetDelegateFromHistoryService(HistoryService* history_service,
                              bool for_transport_mode) {
  if (!history_service) {
    return nullptr;
  }

  // Transport-mode support for HISTORY requires
  // `kReplaceSyncPromosWithSignInPromos`.
  if (for_transport_mode &&
      !syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    return nullptr;
  }
  // The same delegate is used for transport mode and full-sync mode.
  return history_service->GetHistorySyncControllerDelegate();
}

}  // namespace

HistoryDataTypeController::HistoryDataTypeController(
    syncer::SyncService* sync_service,
    HistoryService* history_service,
    PrefService* pref_service)
    : DataTypeController(
          syncer::HISTORY,
          /*delegate_for_full_sync_mode=*/
          GetDelegateFromHistoryService(history_service,
                                        /*for_transport_mode=*/false),
          /*delegate_for_transport_mode=*/
          GetDelegateFromHistoryService(history_service,
                                        /*for_transport_mode=*/true)),
      helper_(syncer::HISTORY,
              sync_service,
              pref_service,
              HistoryDataTypeControllerHelper::AccountManagedStatusPolicy::
                  kDisallowEnterprise),
      history_service_(history_service) {
  sync_observation_.Observe(helper_.sync_service());
}

HistoryDataTypeController::~HistoryDataTypeController() = default;

syncer::DataTypeController::PreconditionState
HistoryDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  // syncer::HISTORY doesn't support custom passphrase encryption.
  if (helper_.sync_service()->GetUserSettings()->IsEncryptEverythingEnabled()) {
    return PreconditionState::kMustStopAndClearData;
  }

  return helper_.GetPreconditionState(context);
}

void HistoryDataTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(helper_.sync_service(), sync);

  // `history_service_` is null in many unit tests.
  if (history_service_) {
    history_service_->SetSyncTransportState(
        helper_.sync_service()->GetTransportState());
  } else {
    CHECK_IS_TEST();
  }
}

void HistoryDataTypeController::OnSyncShutdown(syncer::SyncService* sync) {
  // Nothing to be done, `this` will be destructed imminently.
}

}  // namespace history
