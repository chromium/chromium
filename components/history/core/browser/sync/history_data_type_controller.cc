// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_data_type_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/signin/public/identity_manager/account_info.h"
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
    const signin::AccountManagedStatusFinder* finder) {
  // The finder should generally exist, but if it doesn't, "stop and keep data"
  // is a safe default.
  if (!finder) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndKeepData;
  }

  switch (finder->GetOutcome()) {
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
    signin::IdentityManager* identity_manager,
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
      identity_manager_(identity_manager),
      history_service_(history_service) {
  sync_observation_.Observe(helper_.sync_service());
  CoreAccountInfo account = helper_.sync_service()->GetAccountInfo();
  // If there's already a signed-in account, figure out its "managed" state.
  if (!account.IsEmpty()) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            identity_manager_, account,
            base::BindOnce(&HistoryDataTypeController::AccountTypeDetermined,
                           base::Unretained(this)));
  }
}

HistoryDataTypeController::~HistoryDataTypeController() = default;

syncer::DataTypeController::PreconditionState
HistoryDataTypeController::GetPreconditionState() const {
  // syncer::HISTORY doesn't support custom passphrase encryption.
  if (helper_.sync_service()->GetUserSettings()->IsEncryptEverythingEnabled()) {
    return PreconditionState::kMustStopAndClearData;
  }

  PreconditionState enterprise_state =
      GetPreconditionStateFromManagedStatus(managed_status_finder_.get());

  PreconditionState helper_state = helper_.GetPreconditionState();

  return GetStricterPreconditionState(enterprise_state, helper_state);
}

void HistoryDataTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(helper_.sync_service(), sync);

  // If there wasn't an account previously, or the account has changed, recreate
  // the managed-status finder.
  if (!managed_status_finder_ ||
      managed_status_finder_->GetAccountInfo().account_id !=
          helper_.sync_service()->GetAccountInfo().account_id) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            identity_manager_, helper_.sync_service()->GetAccountInfo(),
            base::BindOnce(&HistoryDataTypeController::AccountTypeDetermined,
                           base::Unretained(this)));
  }

  // `history_service_` is null in many unit tests.
  if (history_service_) {
    history_service_->SetSyncTransportState(
        helper_.sync_service()->GetTransportState());
  } else {
    CHECK_IS_TEST();
  }

  // Most of these calls will be no-ops but SyncService handles that just fine.
  helper_.sync_service()->DataTypePreconditionChanged(type());
}

void HistoryDataTypeController::AccountTypeDetermined() {
  helper_.sync_service()->DataTypePreconditionChanged(type());
}

}  // namespace history
