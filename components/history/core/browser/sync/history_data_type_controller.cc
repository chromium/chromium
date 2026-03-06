// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_data_type_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
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
  if (for_transport_mode && !base::FeatureList::IsEnabled(
                                syncer::kReplaceSyncPromosWithSignInPromos)) {
    return nullptr;
  }
  // The same delegate is used for transport mode and full-sync mode.
  return history_service->GetHistorySyncControllerDelegate();
}

syncer::DataTypeController::PreconditionState
GetPreconditionStateFromManagedStatus(
    signin::AccountManagedStatusFinder::Outcome managed_status) {
  switch (managed_status) {
    case signin::AccountManagedStatusFinder::Outcome::kConsumerGmail:
    case signin::AccountManagedStatusFinder::Outcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown:
    case signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
      // Regular consumer accounts and @google.com accounts are supported.
      return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinder::Outcome::kEnterprise:
      // syncer::HISTORY isn't supported for Dasher a.k.a. enterprise
      // accounts (with the exception of @google.com accounts).
      return syncer::DataTypeController::PreconditionState::
          kMustStopAndClearData;
    case signin::AccountManagedStatusFinder::Outcome::kPending:
    case signin::AccountManagedStatusFinder::Outcome::kError:
    case signin::AccountManagedStatusFinder::Outcome::kTimeout:
      // While the enterprise-ness of the account isn't known yet, or if the
      // detection failed, "stop and keep data" is a safe default.
      return syncer::DataTypeController::PreconditionState::
          kMustStopAndKeepData;
  }
}

// Higher number means more strict.
int GetPreconditionStateStrictness(
    syncer::DataTypeController::PreconditionState state) {
  switch (state) {
    case syncer::DataTypeController::PreconditionState::kMustStopAndClearData:
      return 2;
    case syncer::DataTypeController::PreconditionState::kMustStopAndKeepData:
      return 1;
    case syncer::DataTypeController::PreconditionState::kPreconditionsMet:
      return 0;
  }
}

syncer::DataTypeController::PreconditionState GetStricterPreconditionState(
    syncer::DataTypeController::PreconditionState state1,
    syncer::DataTypeController::PreconditionState state2) {
  if (GetPreconditionStateStrictness(state1) >=
      GetPreconditionStateStrictness(state2)) {
    return state1;
  }
  return state2;
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
      helper_(syncer::HISTORY, sync_service, pref_service),
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

  PreconditionState enterprise_state =
      GetPreconditionStateFromManagedStatus(context.account_managed_status);

  PreconditionState helper_state = helper_.GetPreconditionState(context);

  return GetStricterPreconditionState(enterprise_state, helper_state);
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
