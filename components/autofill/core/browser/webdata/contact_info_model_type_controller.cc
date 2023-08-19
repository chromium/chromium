// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_model_type_controller.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

using PreconditionState = syncer::DataTypeController::PreconditionState;

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
      return PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
      return syncer::kSyncEnableContactInfoDataTypeForDasherGoogleUsers.Get()
                 ? PreconditionState::kPreconditionsMet
                 : PreconditionState::kMustStopAndClearData;
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
  if (!identity_manager.AreRefreshTokensLoaded()) {
    return PreconditionState::kMustStopAndKeepData;
  }
  const AccountCapabilities& capabilities =
      identity_manager.FindExtendedAccountInfo(core_account_info).capabilities;
  // TODO(crbug.com/1423319): Multiple types of child accounts exists, and this
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

ContactInfoModelTypeController::ContactInfoModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager)
    : ModelTypeController(syncer::CONTACT_INFO,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      sync_service_(sync_service),
      identity_manager_(identity_manager) {
  sync_service_observation_.Observe(sync_service_);
  identity_manager_observer_.Observe(identity_manager_);
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
            identity_manager_, sync_service_->GetAccountInfo(),
            base::BindOnce(
                &ContactInfoModelTypeController::AccountTypeDetermined,
                base::Unretained(this)));
  }
}

ContactInfoModelTypeController::~ContactInfoModelTypeController() = default;

PreconditionState ContactInfoModelTypeController::GetPreconditionState() const {
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

void ContactInfoModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(sync, sync_service_);
  // Recreate the status finder when the account has changed.
  if (!managed_status_finder_ ||
      managed_status_finder_->GetAccountInfo().account_id !=
          sync_service_->GetAccountInfo().account_id) {
    managed_status_finder_ =
        std::make_unique<signin::AccountManagedStatusFinder>(
            identity_manager_, sync_service_->GetAccountInfo(),
            base::BindOnce(
                &ContactInfoModelTypeController::AccountTypeDetermined,
                base::Unretained(this)));
  }
  sync_service_->DataTypePreconditionChanged(type());
}

void ContactInfoModelTypeController::OnRefreshTokensLoaded() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ContactInfoModelTypeController::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK(CalledOnValidThread());
  if (info.account_id == sync_service_->GetAccountInfo().account_id) {
    sync_service_->DataTypePreconditionChanged(type());
  }
}

void ContactInfoModelTypeController::AccountTypeDetermined() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace autofill
