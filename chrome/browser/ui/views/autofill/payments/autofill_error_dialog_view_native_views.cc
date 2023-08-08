// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/autofill_error_dialog_view_native_views.h"

#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace autofill {

AutofillErrorDialogViewNativeViews::AutofillErrorDialogViewNativeViews(
    AutofillErrorDialogController* controller)
    : controller_(controller) {
  SetButtons(ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller_->GetButtonLabel());
  SetModalType(ui::MODAL_TYPE_CHILD);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetShowCloseButton(false);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
}

AutofillErrorDialogViewNativeViews::~AutofillErrorDialogViewNativeViews() {
  if (controller_) {
    controller_->OnDismissed();
    controller_ = nullptr;
  }
}

// static
AutofillErrorDialogView* AutofillErrorDialogView::CreateAndShow(
    AutofillErrorDialogController* controller) {
  AutofillErrorDialogViewNativeViews* dialog_view =
      new AutofillErrorDialogViewNativeViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view,
                                              controller->GetWebContents());
  return dialog_view;
}

void AutofillErrorDialogViewNativeViews::Dismiss() {
  if (controller_) {
    controller_->OnDismissed();
    controller_ = nullptr;
  }
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

views::View* AutofillErrorDialogViewNativeViews::GetContentsView() {
  if (!children().empty())
    return this;

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorIcon, ui::kColorAlertHighSeverity,
          gfx::GetDefaultSizeOfVectorIcon(vector_icons::kErrorIcon))));

  auto* label = AddChildView(std::make_unique<views::Label>(
      controller_->GetDescription(),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  return this;
}

void AutofillErrorDialogViewNativeViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(CreateTitleView(
      GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

std::u16string AutofillErrorDialogViewNativeViews::GetWindowTitle() const {
  return controller_->GetTitle();
}

}  // namespace autofill
