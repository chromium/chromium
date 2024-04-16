// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_precondition_checker.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

using PreconditionState = syncer::ModelTypeController::PreconditionState;

// Determines if the `finder`'s account is eligible to use the CONTACT_INFO
// type based on its managed state. Dasher users are not supported.
PreconditionState GetPreconditionStateFromAccountManagedStatus(
    const signin::AccountManagedStatusFinder* finder) {
  // If the feature is enabled, all account types are supported.
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForDasherUsers)) {
    return PreconditionState::kPreconditionsMet;
  }
  // The `finder` should generally exist. But since it is non-obvious when this
  // code is exactly executed, its existence is checked as a safeguard.
  if (!finder) {
    return PreconditionState::kMustStopAndKeepData;
  }
  switch (finder->GetOutcome()) {
    case signin::AccountManagedStatusFinder::Outcome::kNonEnterprise:
    case signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
      return PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinder::Outcome::kEnterprise:
      return PreconditionState::kMustStopAndClearData;
    case signin::AccountManagedStatusFinder::Outcome::kPending:
    case signin::AccountManagedStatusFinder::Outcome::kError:
      // If the account status cannot be determined (immediately), keep the data
      // to prevent redownloding once the status was determined.
      return PreconditionState::kMustStopAndKeepData;
  }
}

// Determines if the `core_account_info` corresponds to a child account. Those
// are excluded from the CONTACT_INFO data type.
// If the status is not available yet, the data is kept to prevent redownloding
// once the status was determined.
PreconditionState GetPreconditionStateFromAccountChildStatus(
    const signin::IdentityManager& identity_manager,
    const CoreAccountInfo& core_account_info) {
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForChildUsers)) {
    return PreconditionState::kPreconditionsMet;
  }
  if (!identity_manager.AreRefreshTokensLoaded()) {
    return PreconditionState::kMustStopAndKeepData;
  }
  const AccountCapabilities& capabilities =
      identity_manager.FindExtendedAccountInfo(core_account_info).capabilities;
  // TODO(crbug.com/40259988): Multiple types of child accounts exists, and this
  // excludes all of them. Once it becomes clear which subset of child accounts
  // actually needs to be excluded, this logic can be relaxed.
  return capabilities.is_subject_to_parental_controls() ==
                 signin::Tribool::kTrue
             ? PreconditionState::kMustStopAndClearData
             : PreconditionState::kPreconditionsMet;
}

PreconditionState GetStricterPreconditionState(PreconditionState a,
                                               PreconditionState b) {
  auto strictness = [](PreconditionState state) {
    switch (state) {
      case PreconditionState::kPreconditionsMet:
        return 0;
      case PreconditionState::kMustStopAndKeepData:
        return 1;
      case PreconditionState::kMustStopAndClearData:
        return 2;
    }
  };
  return strictness(a) > strictness(b) ? a : b;
}

}  // namespace

ContactInfoPreconditionChecker::ContactInfoPreconditionChecker(
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    base::RepeatingClosure on_precondition_changed)
    : sync_service_(CHECK_DEREF(sync_service)),
      identity_manager_(CHECK_DEREF(identity_manager)),
      on_precondition_changed_(std::move(on_precondition_changed)) {
  sync_service_observation_.Observe(&sync_service_.get());
  // When support for child users is not enabled, the identity observer is
  // necessary to react to change in the supervision status.
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForChildUsers)) {
    identity_manager_observer_.Observe(&identity_manager_.get());
  }
  // When support for Dasher users is not enabled, the managed-status of the
  // account needs to be determined.
  // Note that the controller is instantiated even when there's no signed-in
  // account.
  CoreAccountInfo account = sync_service_->GetAccountInfo();
  if (!account.IsEmpty() &&
      !base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForDasherUsers)) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            &identity_manager_.get(), sync_service_->GetAccountInfo(),
            base::BindOnce(
                &ContactInfoPreconditionChecker::AccountTypeDetermined,
                base::Unretained(this)));
  }
}

ContactInfoPreconditionChecker::~ContactInfoPreconditionChecker() = default;

PreconditionState ContactInfoPreconditionChecker::GetPreconditionState() const {
  // Exclude explicit passphrase users.
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase() &&
      !base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers)) {
    return PreconditionState::kMustStopAndClearData;
  }
  // Exclude child and Dasher accounts.
  return GetStricterPreconditionState(
      GetPreconditionStateFromAccountChildStatus(
          *identity_manager_, sync_service_->GetAccountInfo()),
      GetPreconditionStateFromAccountManagedStatus(
          managed_status_finder_.get()));
}

void ContactInfoPreconditionChecker::OnStateChanged(syncer::SyncService* sync) {
  // Recreate the status finder when the account has changed.
  if (!managed_status_finder_ ||
      managed_status_finder_->GetAccountInfo().account_id !=
          sync_service_->GetAccountInfo().account_id) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            &identity_manager_.get(), sync_service_->GetAccountInfo(),
            base::BindOnce(
                &ContactInfoPreconditionChecker::AccountTypeDetermined,
                base::Unretained(this)));
  }
  on_precondition_changed_.Run();
}

void ContactInfoPreconditionChecker::OnRefreshTokensLoaded() {
  on_precondition_changed_.Run();
}

void ContactInfoPreconditionChecker::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (info.account_id == sync_service_->GetAccountInfo().account_id) {
    on_precondition_changed_.Run();
  }
}

void ContactInfoPreconditionChecker::AccountTypeDetermined() {
  on_precondition_changed_.Run();
}

}  // namespace autofill
