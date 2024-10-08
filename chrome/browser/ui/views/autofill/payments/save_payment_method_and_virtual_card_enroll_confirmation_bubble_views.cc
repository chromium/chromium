// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_payment_method_and_virtual_card_enroll_confirmation_bubble_views.h"

#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews(
        views::View* anchor_view,
        content::WebContents* web_contents,
        base::OnceCallback<void(PaymentsBubbleClosedReason)>
            controller_hide_callback,
        SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_hide_callback_(std::move(controller_hide_callback)),
      ui_params_(std::move(ui_params)) {
  if (ui_params_.is_success) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    SetShowCloseButton(true);
  } else {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
    SetButtonLabel(ui::mojom::DialogButton::kOk,
                   ui_params_.failure_ok_button_text);
  }
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::Hide() {
  CloseBubble();
  if (!controller_hide_callback_.is_null()) {
    std::move(controller_hide_callback_)
        .Run(GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::AddedToWidget() {
  if (ui_params_.is_success) {
    auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
        ui::ImageModel::FromVectorIcon(kSaveCardAndVcnSuccessConfirmationIcon),
        ui::ImageModel::FromVectorIcon(
            kSaveCardAndVcnSuccessConfirmationDarkIcon),
        base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                            base::Unretained(this)));
    image_view->SetBorder(
        views::CreateEmptyBorder(ChromeLayoutProvider::Get()
                                     ->GetInsetsMetric(views::INSETS_DIALOG)
                                     .set_bottom(0)));
    GetBubbleFrameView()->SetHeaderView(std::move(image_view));
  }
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAfterLabelView>(
          GetWindowTitle(), TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
}

std::u16string
SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::GetWindowTitle() const {
  return ui_params_.title_text;
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::WindowClosing() {
  if (!controller_hide_callback_.is_null()) {
    std::move(controller_hide_callback_)
        .Run(GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    OnWidgetInitialized() {
  if (auto* ok_button = GetOkButton()) {
    ok_button->GetViewAccessibility().SetName(
        ui_params_.failure_ok_button_accessible_name);
  }
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  auto description = std::make_unique<views::Label>(
      ui_params_.description_text, views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  description->SetID(DialogViewId::DESCRIPTION_LABEL);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description->SetMultiLine(true);
  description->GetViewAccessibility().SetName(ui_params_.description_text);
  AddChildView(std::move(description));
}

SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    ~SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews() = default;

}  // namespace autofill
