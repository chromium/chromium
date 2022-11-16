// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"

#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

EditAddressProfileView::EditAddressProfileView(
    EditAddressProfileDialogController* controller)
    : controller_(controller) {
  DCHECK(controller);

  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetAcceptCallback(base::BindOnce(
      &EditAddressProfileView::OnUserDecision, base::Unretained(this),
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted));
  SetCancelCallback(base::BindOnce(
      &EditAddressProfileView::OnUserDecision, base::Unretained(this),
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditDeclined));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_margins(ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG));

  SetTitle(controller_->GetWindowTitle());
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_CANCEL_BUTTON_LABEL));
}

EditAddressProfileView::~EditAddressProfileView() = default;

void EditAddressProfileView::ShowForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  address_editor_controller_ = std::make_unique<AddressEditorController>(
      controller_->GetProfileToEdit(), web_contents);
  address_editor_view_ = AddChildView(
      std::make_unique<AddressEditorView>(address_editor_controller_.get()));
}

void EditAddressProfileView::Hide() {
  controller_ = nullptr;
  GetWidget()->Close();
}

void EditAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

void EditAddressProfileView::ChildPreferredSizeChanged(views::View* child) {
  const int width = fixed_width();
  GetWidget()->SetSize(gfx::Size(width, GetHeightForWidth(width)));
}

AddressEditorView* EditAddressProfileView::GetAddressEditorViewForTesting() {
  return address_editor_view_;
}

void EditAddressProfileView::OnUserDecision(
    AutofillClient::SaveAddressProfileOfferUserDecision decision) {
  if (!controller_)
    return;
  controller_->OnUserDecision(decision,
                              address_editor_view_->GetAddressProfile());
}

}  // namespace autofill
