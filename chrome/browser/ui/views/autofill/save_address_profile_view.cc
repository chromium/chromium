// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ui/gfx/color_utils.h"

namespace autofill {

SaveAddressProfileView::SaveAddressProfileView(
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

void SaveAddressProfileView::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  // TODO(crbug.com/1167060): Update upon having final mocks.
  int id = color_utils::IsDark(GetBubbleFrameView()->GetBackgroundColor())
               ? IDR_SAVE_PASSWORD_MULTI_DEVICE_DARK
               : IDR_SAVE_PASSWORD_MULTI_DEVICE;

  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetImage(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id));
  GetBubbleFrameView()->SetHeaderView(std::move(image_view));
}

}  // namespace autofill
