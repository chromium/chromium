// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/sync_utils/plus_address_data_type_controller.h"

#include "base/check_deref.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"
#include "components/variations/service/google_groups_manager.h"

namespace plus_addresses {

namespace {

using PreconditionState = syncer::DataTypeController::PreconditionState;

// Determines the eligibility of the data type based on the accounts managed
// status. Dasher accounts are not supported.
PreconditionState GetPreconditionStateFromAccountManagedStatus(
    signin::AccountManagedStatusFinderOutcome status) {
  switch (status) {
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

PlusAddressDataTypeController::PlusAddressDataTypeController(
    syncer::DataType type,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode,
    GoogleGroupsManager* google_groups_manager)
    : syncer::DataTypeController(type,
                                 std::move(delegate_for_full_sync_mode),
                                 std::move(delegate_for_transport_mode)),
      google_groups_manager_(CHECK_DEREF(google_groups_manager)) {
  CHECK(type == syncer::DataType::PLUS_ADDRESS ||
        type == syncer::DataType::PLUS_ADDRESS_SETTING);
}

PlusAddressDataTypeController::~PlusAddressDataTypeController() = default;

PreconditionState PlusAddressDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  if (!google_groups_manager_->IsFeatureEnabledForProfile(
          plus_addresses::features::kPlusAddressesEnabled) ||
      plus_addresses::features::kEnterprisePlusAddressServerUrl.Get().empty()) {
    return PreconditionState::kMustStopAndClearData;
  }
  return GetPreconditionStateFromAccountManagedStatus(
      context.account_managed_status);
}

}  // namespace plus_addresses
