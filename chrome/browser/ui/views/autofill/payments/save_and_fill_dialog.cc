// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog.h"

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace autofill {

SaveAndFillDialog::SaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller)
    : controller_(controller) {
  // Set the ownership of the delegate, not the View. The View is owned by the
  // Widget as a child view.
  // TODO(crbug.com/338254375): Remove the following line once this is the
  // default state for widgets.
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  SetModalType(ui::mojom::ModalType::kChild);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 controller_->GetAcceptButtonText());
  SetShowCloseButton(false);
  InitViews();
}

SaveAndFillDialog::~SaveAndFillDialog() = default;

void SaveAndFillDialog::AddedToWidget() {
  if (controller_->IsUploadSaveAndFill()) {
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  } else {
    auto title_view = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title_view->SetMultiLine(true);
    GetBubbleFrameView()->SetTitleView(std::move(title_view));
  }
}

std::u16string SaveAndFillDialog::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveAndFillDialog::InitViews() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  AddChildView(views::Builder<views::Label>()
                   .SetText(controller_->GetExplanatoryMessage())
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                   .Build());
  // Create a container for the card number label and textfield.
  AddChildView(CreateLabelAndTextfieldView(controller_->GetCardNumberLabel()));
  // Create a container for the cardholder name label and textfield.
  AddChildView(CreateLabelAndTextfieldView(controller_->GetNameOnCardLabel()));
}

}  // namespace autofill
