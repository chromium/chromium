// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/edit_address_profile_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/address_editor_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/edit_address_profile_dialog_controller.h"
#include "chrome/browser/ui/autofill/edit_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/address_editor_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AutofillBubbleBase* ShowEditAddressProfileDialogView(
    content::WebContents* web_contents,
    EditAddressProfileDialogController* controller) {
  EditAddressProfileView* dialog = new EditAddressProfileView(controller);
  dialog->ShowForWebContents(web_contents);
  constrained_window::ShowWebModalDialogViews(dialog, web_contents);
  dialog->RequestFocus();
  return dialog;
}

EditAddressProfileView::EditAddressProfileView(
    EditAddressProfileDialogController* controller)
    : controller_(controller) {
  DCHECK(controller);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetAcceptCallbackWithClose(base::BindRepeating(
      &EditAddressProfileView::OnAcceptButtonClicked, base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      &EditAddressProfileView::OnUserDecision, base::Unretained(this),
      AutofillClient::AddressPromptUserDecision::kEditDeclined));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  set_margins(ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG));

  SetProperty(views::kElementIdentifierKey, kTopViewId);
  SetTitle(controller_->GetWindowTitle());
  SetButtonLabel(ui::mojom::DialogButton::kOk, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_EDIT_ADDRESS_DIALOG_CANCEL_BUTTON_LABEL));
}

EditAddressProfileView::~EditAddressProfileView() = default;

void EditAddressProfileView::ShowForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto address_editor_controller = std::make_unique<AddressEditorController>(
      controller_->GetProfileToEdit(),
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()),
      controller_->GetIsValidatable());

  // Storing subscription (which gets canceled in the destructor) in a property
  // secures using of Unretained(this).
  on_is_valid_change_subscription_ =
      address_editor_controller->AddIsValidChangedCallback(
          base::BindRepeating(&EditAddressProfileView::UpdateActionButtonState,
                              base::Unretained(this)));

  address_editor_view_ = AddChildView(std::make_unique<AddressEditorView>(
      std::move(address_editor_controller)));

  const std::u16string& footer_message = controller_->GetFooterMessage();
  if (!footer_message.empty()) {
    AddChildView(
        views::Builder<views::Label>()
            .SetText(footer_message)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetMultiLine(true)
            .Build());
  }
}

void EditAddressProfileView::Hide() {
  controller_ = nullptr;
  GetWidget()->Close();
}

views::View* EditAddressProfileView::GetInitiallyFocusedView() {
  return address_editor_view_ ? address_editor_view_->initial_focus_view()
                              : nullptr;
}

void EditAddressProfileView::WindowClosing() {
  if (controller_) {
    controller_->OnDialogClosed(
        decision_,
        decision_ == AutofillClient::AddressPromptUserDecision::kEditAccepted
            ? base::optional_ref(address_editor_view_->GetAddressProfile())
            : std::nullopt);
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
    AutofillClient::AddressPromptUserDecision decision) {
  decision_ = decision;
}

void EditAddressProfileView::UpdateActionButtonState(bool is_valid) {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, is_valid);
}

bool EditAddressProfileView::OnAcceptButtonClicked() {
  bool is_form_valid = address_editor_view_->ValidateAllFields();
  if (is_form_valid) {
    OnUserDecision(AutofillClient::AddressPromptUserDecision::kEditAccepted);
  }
  return is_form_valid;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(EditAddressProfileView, kTopViewId);

}  // namespace autofill
