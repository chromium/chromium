// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/data_type_controller/managed_account_precondition_checker.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_user_settings.h"

namespace data_sharing {
namespace {

using PreconditionState = syncer::DataTypeController::PreconditionState;

// Determines if the `finder`'s account is eligible to use the
// SHARED_TAB_GROUP_DATA type based on its managed state. Dasher users are
// currently not supported.
PreconditionState GetPreconditionStateFromAccountManagedStatus(
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
      // Regular consumer accounts are supported.
      return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
    case signin::AccountManagedStatusFinder::Outcome::kEnterprise:
      // Not supported for Dasher a.k.a. enterprise accounts (including
      // @google.com accounts).
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

}  // namespace

ManagedAccountPreconditionChecker::ManagedAccountPreconditionChecker(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    base::RepeatingClosure on_precondition_changed)
    : sync_service_(CHECK_DEREF(sync_service)),
      identity_manager_(CHECK_DEREF(identity_manager)),
      on_precondition_changed_(std::move(on_precondition_changed)) {
  sync_service_observation_.Observe(&sync_service_.get());
  // If there's already a signed-in account, figure out its "managed" state.
  CoreAccountInfo account = sync_service_->GetAccountInfo();
  if (!account.IsEmpty()) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            &identity_manager_.get(), sync_service_->GetAccountInfo(),
            base::BindOnce(
                &ManagedAccountPreconditionChecker::AccountTypeDetermined,
                base::Unretained(this)));
  }
}

ManagedAccountPreconditionChecker::~ManagedAccountPreconditionChecker() =
    default;

PreconditionState ManagedAccountPreconditionChecker::GetPreconditionState()
    const {
  // Exclude Dasher accounts.
  return GetPreconditionStateFromAccountManagedStatus(
      managed_status_finder_.get());
}

void ManagedAccountPreconditionChecker::OnStateChanged(
    syncer::SyncService* sync) {
  // If there wasn't an account previously, or the account has changed, recreate
  // the managed-status finder.
  if (!managed_status_finder_ ||
      managed_status_finder_->GetAccountInfo().account_id !=
          sync_service_->GetAccountInfo().account_id) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            &identity_manager_.get(), sync_service_->GetAccountInfo(),
            base::BindOnce(
                &ManagedAccountPreconditionChecker::AccountTypeDetermined,
                base::Unretained(this)));
  }
  on_precondition_changed_.Run();
}

void ManagedAccountPreconditionChecker::AccountTypeDetermined() {
  on_precondition_changed_.Run();
}

}  // namespace data_sharing
