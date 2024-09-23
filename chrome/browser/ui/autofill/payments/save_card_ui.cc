// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_ui.h"

#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

UpdatedDesktopUiTreatmentArm GetUpdatedDesktopUiTreatmentArm() {
  if (!base::FeatureList::IsEnabled(features::kAutofillUpstreamUpdatedUi)) {
    return UpdatedDesktopUiTreatmentArm::kDefault;
  }

  switch (features::kAutofillUpstreamUpdatedUiTreatment.Get()) {
    case 1:
      return UpdatedDesktopUiTreatmentArm::kSecurityFocus;
    case 2:
      return UpdatedDesktopUiTreatmentArm::kConvenienceFocus;
    case 3:
      return UpdatedDesktopUiTreatmentArm::kEducationFocus;
    default:
      return UpdatedDesktopUiTreatmentArm::kDefault;
  }
}

}  // namespace autofill
