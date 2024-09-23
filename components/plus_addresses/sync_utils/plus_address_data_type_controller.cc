// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/sync_utils/plus_address_data_type_controller.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/variations/service/google_groups_manager.h"

namespace plus_addresses {

namespace {

using PreconditionState = syncer::DataTypeController::PreconditionState;

// Determines the eligibility of the data type based on the accounts managed
// status. Dasher accounts are not supported.
PreconditionState GetPreconditionStateFromAccountManagedStatus(
    const signin::AccountManagedStatusFinder& finder) {
  switch (finder.GetOutcome()) {
    case signin::AccountManagedStatusFinder::Outcome::kConsumerGmail:
    case signin::AccountManagedStatusFinder::Outcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown:
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

}  // namespace

PlusAddressDataTypeController::PlusAddressDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    signin::IdentityManager* identity_manager,
    GoogleGroupsManager* google_groups_manager)
    : syncer::DataTypeController(type,
                                 std::move(delegate_for_full_sync_mode),
                                 std::move(delegate_for_transport_mode)),
      sync_service_(sync_service),
      identity_manager_(identity_manager),
      google_groups_manager_(CHECK_DEREF(google_groups_manager)) {
  CHECK(type == syncer::DataType::PLUS_ADDRESS ||
        type == syncer::DataType::PLUS_ADDRESS_SETTING);
  sync_service_observation_.Observe(sync_service_);
  RecreateManagedStatusFinder();
}

PlusAddressDataTypeController::~PlusAddressDataTypeController() = default;

PreconditionState PlusAddressDataTypeController::GetPreconditionState() const {
  if (!google_groups_manager_->IsFeatureEnabledForProfile(
          plus_addresses::features::kPlusAddressesEnabled) ||
      plus_addresses::features::kEnterprisePlusAddressServerUrl.Get().empty()) {
    return PreconditionState::kMustStopAndClearData;
  }
  return GetPreconditionStateFromAccountManagedStatus(*managed_status_finder_);
}

void PlusAddressDataTypeController::OnStateChanged(syncer::SyncService*) {
  if (managed_status_finder_->GetAccountInfo().account_id !=
      sync_service_->GetAccountInfo().account_id) {
    RecreateManagedStatusFinder();
    // Recreating changes the `managed_status_finder_->GetOutcome()` to
    // `kPending` briefly, until `OnAccountTypeDetermined()` is called.
    sync_service_->DataTypePreconditionChanged(type());
  }
}

void PlusAddressDataTypeController::RecreateManagedStatusFinder() {
  managed_status_finder_ = std::make_unique<signin::AccountManagedStatusFinder>(
      identity_manager_, sync_service_->GetAccountInfo(),
      base::BindOnce(&PlusAddressDataTypeController::OnAccountTypeDetermined,
                     base::Unretained(this)));
}

void PlusAddressDataTypeController::OnAccountTypeDetermined() {
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace plus_addresses
