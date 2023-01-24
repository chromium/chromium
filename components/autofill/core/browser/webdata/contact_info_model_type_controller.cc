// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/contact_info_model_type_controller.h"

#include "base/feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/model_type.h"

namespace autofill {

ContactInfoModelTypeController::ContactInfoModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : ModelTypeController(syncer::CONTACT_INFO,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)) {}

bool ContactInfoModelTypeController::ShouldRunInTransportOnlyMode() const {
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillAccountProfilesOnSignIn);
}

}  // namespace autofill
