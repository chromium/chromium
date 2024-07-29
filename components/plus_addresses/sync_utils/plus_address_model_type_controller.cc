// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/sync_utils/plus_address_model_type_controller.h"

#include "base/check_deref.h"
#include "components/plus_addresses/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/model_type_controller.h"
#include "components/variations/service/google_groups_manager.h"

namespace plus_addresses {

PlusAddressModelTypeController::PlusAddressModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    GoogleGroupsManager* google_groups_manager)
    : syncer::ModelTypeController(type,
                                  std::move(delegate_for_full_sync_mode),
                                  std::move(delegate_for_transport_mode)),
      google_groups_manager_(CHECK_DEREF(google_groups_manager)) {
  CHECK(type == syncer::ModelType::PLUS_ADDRESS ||
        type == syncer::ModelType::PLUS_ADDRESS_SETTING);
}

syncer::ModelTypeController::PreconditionState
PlusAddressModelTypeController::GetPreconditionState() const {
  using PreconditionState = syncer::ModelTypeController::PreconditionState;
  if (!google_groups_manager_->IsFeatureEnabledForProfile(
          plus_addresses::features::kPlusAddressesEnabled) ||
      plus_addresses::features::kEnterprisePlusAddressServerUrl.Get().empty()) {
    return PreconditionState::kMustStopAndClearData;
  }
  return PreconditionState::kPreconditionsMet;
}

}  // namespace plus_addresses
