// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_selection_dialog_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_controller.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace autofill {

VirtualCardSelectionDialogView::VirtualCardSelectionDialogView(
    VirtualCardSelectionDialogController* controller)
    : controller_(controller) {
  SetShowTitle(true);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetOkButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller_->GetCancelButtonLabel());
  SetAcceptCallback(
      base::BindOnce(&VirtualCardSelectionDialogController::OnOkButtonClicked,
                     base::Unretained(controller_)));
  SetCancelCallback(base::BindOnce(
      &VirtualCardSelectionDialogController::OnCancelButtonClicked,
      base::Unretained(controller_)));

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

VirtualCardSelectionDialogView::~VirtualCardSelectionDialogView() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

// static
VirtualCardSelectionDialog* VirtualCardSelectionDialog::CreateAndShow(
    VirtualCardSelectionDialogController* controller,
    content::WebContents* web_content) {
  VirtualCardSelectionDialogView* dialog =
      new VirtualCardSelectionDialogView(controller);
  constrained_window::ShowWebModalDialogViews(dialog, web_content);
  return dialog;
}

void VirtualCardSelectionDialogView::Hide() {
  // Reset controller reference if the controller has been destroyed before the
  // view being destroyed. This happens if browser window is closed when the
  // dialog is visible.
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

void VirtualCardSelectionDialogView::AddedToWidget() {
  // TODO(crbug.com/1020740): The header image is not ready. Implement it later.
}

bool VirtualCardSelectionDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? controller_->IsOkButtonEnabled()
                                        : true;
}

views::View* VirtualCardSelectionDialogView::GetContentsView() {
  RemoveAllChildViews();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText,
          views::DialogContentType::kControl)));

  auto* instructions = AddChildView(std::make_unique<views::Label>(
      controller_->GetContentExplanation(),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  instructions->SetMultiLine(true);
  instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // TODO(crbug.com/1020740): Add the card list in a separate CL.

  return this;
}

std::u16string VirtualCardSelectionDialogView::GetWindowTitle() const {
  return controller_->GetContentTitle();
}

BEGIN_METADATA(VirtualCardSelectionDialogView, views::DialogDelegateView)
END_METADATA

}  // namespace autofill
