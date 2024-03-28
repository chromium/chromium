// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_model_type_controller.h"

#include <memory>

#include "base/feature_list.h"
#include "components/plus_addresses/features.h"
#include "components/sync/base/model_type.h"

namespace plus_addresses {

PlusAddressModelTypeController::PlusAddressModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : syncer::ModelTypeController(syncer::PLUS_ADDRESS,
                                  std::move(delegate_for_full_sync_mode),
                                  std::move(delegate_for_transport_mode)) {}

PlusAddressModelTypeController::~PlusAddressModelTypeController() = default;

syncer::ModelTypeController::PreconditionState
PlusAddressModelTypeController::GetPreconditionState() const {
  // TODO(b/322147254): Finch enrolment state is browser-wide, not profile-wide.
  // Make sure that PLUS_ADDRESS is only enabled for eligible profiles.
  return base::FeatureList::IsEnabled(features::kFeature)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

}  // namespace plus_addresses
