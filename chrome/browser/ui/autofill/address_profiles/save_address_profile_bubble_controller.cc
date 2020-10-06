// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_profiles/save_address_profile_bubble_controller.h"

#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfileBubbleController::SaveAddressProfileBubbleController() {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

SaveAddressProfileBubbleController::~SaveAddressProfileBubbleController() =
    default;

base::string16 SaveAddressProfileBubbleController::GetWindowTitle() const {
  return base::string16();
}

}  // namespace autofill
