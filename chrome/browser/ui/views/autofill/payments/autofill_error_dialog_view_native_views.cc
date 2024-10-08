// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/autofill_error_dialog_view_native_views.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

AutofillErrorDialogViewNativeViews::AutofillErrorDialogViewNativeViews(
    AutofillErrorDialogController* controller)
    : controller_(controller->GetWeakPtr()) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 controller_->GetButtonLabel());
  SetModalType(ui::mojom::ModalType::kChild);
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
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* icon = AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kErrorIcon, ui::kColorAlertHighSeverity,
          gfx::GetDefaultSizeOfVectorIcon(vector_icons::kErrorIcon))));

  if (controller_) {
    auto* label = AddChildView(std::make_unique<views::Label>(
        controller_->GetDescription(),
        ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SetMultiLine(true);

    // Center-align the error icon vertically with the first line of the label.
    icon->SetBorder(views::CreateEmptyBorder(gfx::Insets().set_top(
        (label->GetLineHeight() - icon->GetPreferredSize().height()) / 2)));
  }

  return this;
}

void AutofillErrorDialogViewNativeViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

std::u16string AutofillErrorDialogViewNativeViews::GetWindowTitle() const {
  return controller_ ? controller_->GetTitle() : std::u16string();
}

base::WeakPtr<AutofillErrorDialogView>
AutofillErrorDialogViewNativeViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<AutofillErrorDialogView> CreateAndShowAutofillErrorDialog(
    AutofillErrorDialogController* controller,
    content::WebContents* web_contents) {
  AutofillErrorDialogViewNativeViews* dialog_view =
      new AutofillErrorDialogViewNativeViews(controller);
  constrained_window::ShowWebModalDialogViews(dialog_view, web_contents);
  return dialog_view->GetWeakPtr();
}

}  // namespace autofill
