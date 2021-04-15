// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

UpdateAddressProfileView::UpdateAddressProfileView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    SaveAddressProfileBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));
  SetAcceptCallback(base::BindOnce(
      &SaveAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  SetCancelCallback(base::BindOnce(
      &SaveAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_),
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));
}

bool UpdateAddressProfileView::ShouldShowCloseButton() const {
  return true;
}

std::u16string UpdateAddressProfileView::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void UpdateAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

void UpdateAddressProfileView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void UpdateAddressProfileView::Hide() {
  CloseBubble();

  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();

  controller_ = nullptr;
}

void UpdateAddressProfileView::AddedToWidget() {
  // TODO(crbug.com/1167060): Update upon having final mocks.
  // Set the header image.
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_SAVE_PASSWORD_MULTI_DEVICE),
      *bundle.GetImageSkiaNamed(IDR_SAVE_PASSWORD_MULTI_DEVICE_DARK),
      base::BindRepeating(&views::BubbleFrameView::GetBackgroundColor,
                          base::Unretained(GetBubbleFrameView())));
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

}  // namespace autofill
