// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"

#include "chrome/browser/ui/autofill/save_address_profile_bubble_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/constrained_window/constrained_window_views.h"

namespace autofill {

EditAddressProfileView::EditAddressProfileView(
    content::WebContents* web_contents,
    SaveAddressProfileBubbleController* controller)
    : controller_(controller) {
  DCHECK(controller);
  DCHECK(web_contents);
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillAddressProfileSavePrompt));

  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetAcceptCallback(base::BindOnce(
      &SaveAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_.get()),
      AutofillClient::SaveAddressProfileOfferUserDecision::kAccepted));
  SetCancelCallback(base::BindOnce(
      &SaveAddressProfileBubbleController::OnUserDecision,
      base::Unretained(controller_.get()),
      AutofillClient::SaveAddressProfileOfferUserDecision::kDeclined));

  constrained_window::ShowWebModalDialogViews(this, web_contents);
}

void EditAddressProfileView::Hide() {
  controller_ = nullptr;
  GetWidget()->Close();
}

base::string16 EditAddressProfileView::GetWindowTitle() const {
  // |controller_| can be nullptr when the framework calls this method after a
  // button click.
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

void EditAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnEditDialogClosed();
    controller_ = nullptr;
  }
}

}  // namespace autofill
