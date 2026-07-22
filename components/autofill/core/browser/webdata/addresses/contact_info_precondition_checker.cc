// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/contact_info_precondition_checker.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/sync/base/features.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

namespace {

using PreconditionState = syncer::DataTypeController::PreconditionState;

// Determines if the signed-in account is eligible to use the CONTACT_INFO
// type based on its managed state. Dasher users are not supported.
PreconditionState GetPreconditionStateFromAccountManagedStatus(
    signin::AccountManagedStatusFinderOutcome account_managed_status) {
  // If the feature is enabled, all account types are supported.
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeForDasherUsers)) {
    return PreconditionState::kPreconditionsMet;
  }
  switch (account_managed_status) {
    case signin::AccountManagedStatusFinderOutcome::kConsumerGmail:
    case signin::AccountManagedStatusFinderOutcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kConsumerNotWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom:
      return PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinderOutcome::kEnterprise:
      return PreconditionState::kMustStopAndClearData;
    case signin::AccountManagedStatusFinderOutcome::kPending:
    case signin::AccountManagedStatusFinderOutcome::kError:
    case signin::AccountManagedStatusFinderOutcome::kTimeout:
      // If the account status cannot be determined (immediately), keep the data
      // to prevent redownloding once the status was determined.
      return PreconditionState::kMustStopAndKeepData;
  }
}

}  // namespace

ContactInfoPreconditionChecker::ContactInfoPreconditionChecker(
    syncer::SyncService* sync_service,
    base::RepeatingClosure on_precondition_changed)
    : on_precondition_changed_(std::move(on_precondition_changed)) {
  CHECK(sync_service);
  sync_service_observation_.Observe(sync_service);
}

ContactInfoPreconditionChecker::~ContactInfoPreconditionChecker() = default;

PreconditionState ContactInfoPreconditionChecker::GetPreconditionState(
    const syncer::DataTypeController::PreconditionContext& context) const {
  const syncer::SyncService* sync_service = GetSyncService();
  // Can happen if this gets called after `OnSyncShutdown()` - in that case,
  // "stop and keep data" is a safe default.
  if (!sync_service) {
    return PreconditionState::kMustStopAndKeepData;
  }
  // Exclude explicit passphrase users.
  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase() &&
      !syncer::IsContactInfoDataTypeForCustomPassphraseUsersEnabled()) {
    return PreconditionState::kMustStopAndClearData;
  }
  // Exclude Dasher accounts.
  return GetPreconditionStateFromAccountManagedStatus(
      context.account_managed_status);
}

void ContactInfoPreconditionChecker::OnStateChanged(syncer::SyncService* sync) {
  CHECK_EQ(sync, GetSyncService());
  on_precondition_changed_.Run();
}

void ContactInfoPreconditionChecker::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observation_.Reset();
}

const syncer::SyncService* ContactInfoPreconditionChecker::GetSyncService()
    const {
  return sync_service_observation_.GetSource();
}

}  // namespace autofill
