// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_payment_method_and_virtual_card_enroll_confirmation_bubble_views.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/browser_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/styled_label.h"

namespace autofill {

SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews(
        views::BubbleAnchor anchor,
        content::WebContents* web_contents,
        base::OnceCallback<void(PaymentsUiClosedReason)>
            controller_hide_callback,
        SavePaymentMethodAndVirtualCardEnrollConfirmationUiParams ui_params)
    : AutofillLocationBarBubble(anchor, web_contents),
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
        .Run(GetPaymentsUiClosedReasonFromWidget(GetWidget()));
  }
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    AddedToWidget() {
  if (ui_params_.is_success) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    auto image =
        std::make_unique<views::ImageView>(bundle.GetThemedLottieImageNamed(
            IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_SUCCESS_LOTTIE));
    image->GetViewAccessibility().SetIsInvisible(true);
    image->SetBorder(
        views::CreateEmptyBorder(ChromeLayoutProvider::Get()
                                     ->GetInsetsMetric(views::INSETS_DIALOG)
                                     .set_bottom(0)));
    GetBubbleFrameView()->SetHeaderView(std::move(image));
  }

  bool is_wallet_branding_enabled =
      base::FeatureList::IsEnabled(features::kAutofillEnableWalletBranding);
  bool should_show_logo = ui_params_.is_success
                              ? ui_params_.should_display_wallet_logo
                              : !is_wallet_branding_enabled;
  if (should_show_logo) {
    GetBubbleFrameView()->SetTitleView(
        std::make_unique<TitleWithIconAfterLabelView>(
            GetWindowTitle(),
            is_wallet_branding_enabled
                ? TitleWithIconAfterLabelView::Icon::GOOGLE_WALLET
                : TitleWithIconAfterLabelView::Icon::GOOGLE_PAY));
  } else {
    // The Google Wallet logo should not be shown for failed upload saves or for
    // successful upload saves specifically made via the Save and Fill feature.
    auto title_view = std::make_unique<views::Label>(
        GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
    title_view->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    title_view->SetMultiLine(true);
    GetBubbleFrameView()->SetTitleView(std::move(title_view));
  }
}

std::u16string
SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::GetWindowTitle()
    const {
  return ui_params_.title_text;
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    WindowClosing() {
  if (!controller_hide_callback_.is_null()) {
    std::move(controller_hide_callback_)
        .Run(GetPaymentsUiClosedReasonFromWidget(GetWidget()));
  }
}

void SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetID(
      DialogViewId::
          SAVE_PAYMENT_METHOD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_BUBBLE_VIEWS);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  auto description = std::make_unique<views::StyledLabel>();
  description->SetText(ui_params_.description_text);
  description->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  description->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  description->SetID(DialogViewId::DESCRIPTION_LABEL);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description->GetViewAccessibility().SetName(ui_params_.description_text);
  if (ui_params_.description_text_link_range_and_callback.has_value()) {
    views::StyledLabel::RangeStyleInfo style_info =
        views::StyledLabel::RangeStyleInfo::CreateForLink(std::get<2>(
            ui_params_.description_text_link_range_and_callback.value()));
    description->AddStyleRange(
        gfx::Range(
            std::get<0>(
                ui_params_.description_text_link_range_and_callback.value())
                .value(),
            std::get<1>(
                ui_params_.description_text_link_range_and_callback.value())
                .value()),
        style_info);
  }
  AddChildView(std::move(description));
}

SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews::
    ~SavePaymentMethodAndVirtualCardEnrollConfirmationBubbleViews() = default;

}  // namespace autofill
