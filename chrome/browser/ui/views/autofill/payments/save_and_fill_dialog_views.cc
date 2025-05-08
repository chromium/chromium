// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_dialog_views.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace autofill {

SaveAndFillDialogViews::SaveAndFillDialogViews(
    base::WeakPtr<SaveAndFillDialogController> controller)
    : controller_(controller) {
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

SaveAndFillDialogViews::~SaveAndFillDialogViews() = default;

base::WeakPtr<SaveAndFillDialogView> SaveAndFillDialogViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<SaveAndFillDialogView> CreateAndShowSaveAndFillDialog(
    base::WeakPtr<SaveAndFillDialogController> controller,
    content::WebContents* web_contents) {
  SaveAndFillDialogViews* dialog_view = new SaveAndFillDialogViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view->GetWeakPtr();
}

void SaveAndFillDialogViews::AddedToWidget() {
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

std::u16string SaveAndFillDialogViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void SaveAndFillDialogViews::InitViews() {
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
