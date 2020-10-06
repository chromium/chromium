// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_profiles/save_address_profile_view.h"

#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfileView::SaveAddressProfileView(
    views::View* anchor_view,
    content::WebContents* web_contents)
    : LocationBarBubbleDelegateView(anchor_view, web_contents) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

bool SaveAddressProfileView::ShouldShowCloseButton() const {
  return true;
}

base::string16 SaveAddressProfileView::GetWindowTitle() const {
  return base::string16();
}

}  // namespace autofill
