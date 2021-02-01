// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_profiles/save_address_profile_view.h"

#include "chrome/browser/ui/autofill/address_profiles/save_address_profile_bubble_controller.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

SaveAddressProfileView::SaveAddressProfileView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveAddressProfileBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
}

bool SaveAddressProfileView::ShouldShowCloseButton() const {
  return true;
}

base::string16 SaveAddressProfileView::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SaveAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void SaveAddressProfileView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SaveAddressProfileView::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();

  controller_ = nullptr;
}

}  // namespace autofill
