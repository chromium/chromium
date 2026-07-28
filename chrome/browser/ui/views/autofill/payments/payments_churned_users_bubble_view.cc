// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_churned_users_bubble_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/browser_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

PaymentsChurnedUsersBubbleView::PaymentsChurnedUsersBubbleView(
    views::BubbleAnchor anchor,
    content::WebContents* web_contents,
    PaymentsChurnedUsersBubbleController* controller)
    : AutofillLocationBarBubble(anchor, web_contents), controller_(controller) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_CHURNED_USERS_BUBBLE_ACCEPT_BUTTON_LABEL));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_AUTOFILL_CHURNED_USERS_BUBBLE_CANCEL_BUTTON_LABEL));
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

PaymentsChurnedUsersBubbleView::~PaymentsChurnedUsersBubbleView() = default;

void PaymentsChurnedUsersBubbleView::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void PaymentsChurnedUsersBubbleView::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsUiClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void PaymentsChurnedUsersBubbleView::AddedToWidget() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  int resource_id;
  switch (
      controller_->GetAutofillEnableResurrectingPaymentsUsersTreatmentArm()) {
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity: {
      resource_id = IDR_AUTOFILL_SAVE_CARD_SECURE_LOTTIE;
      break;
    }
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience: {
      resource_id = IDR_AUTOFILL_TURN_ON_PAYMENTS_AUTOFILL_CONVENIENTLY_LOTTIE;
      break;
    }
  }
  auto image = std::make_unique<views::ImageView>(
      bundle.GetThemedLottieImageNamed(resource_id));
  image->GetViewAccessibility().SetIsInvisible(true);
  image->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()
                                   ->GetInsetsMetric(views::INSETS_DIALOG)
                                   .set_bottom(0)));
  GetBubbleFrameView()->SetHeaderView(std::move(image));
}

std::u16string PaymentsChurnedUsersBubbleView::GetWindowTitle() const {
  switch (
      controller_->GetAutofillEnableResurrectingPaymentsUsersTreatmentArm()) {
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity: {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CHURNED_USERS_BUBBLE_SECURITY_TITLE);
    }
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience: {
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CHURNED_USERS_BUBBLE_CONVENIENCE_TITLE);
    }
  }
}

void PaymentsChurnedUsersBubbleView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsUiClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void PaymentsChurnedUsersBubbleView::Init() {
  SetUseDefaultFillLayout(true);

  auto main_content_view = std::make_unique<views::BoxLayoutView>();
  main_content_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  main_content_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));

  std::unique_ptr<views::Label> description;
  switch (
      controller_->GetAutofillEnableResurrectingPaymentsUsersTreatmentArm()) {
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kSecurity: {
      description = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CHURNED_USERS_BUBBLE_SECURITY_DESCRIPTION));
      break;
    }
    case AutofillEnableResurrectingPaymentsUsersTreatmentArm::kConvenience: {
      description = std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CHURNED_USERS_BUBBLE_CONVENIENCE_DESCRIPTION));
      break;
    }
  }
  description->SetMultiLine(true);
  description->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  description->SetTextStyle(views::style::STYLE_SECONDARY);
  main_content_view->AddChildView(std::move(description));

  AccountInfo account_info = controller_->GetAccountInfo();
  main_content_view->AddChildView(CreateUserAvatarAndEmailView(
      base::UTF8ToUTF16(account_info.email), GetProfileAvatar(account_info)));

  AddChildView(std::move(main_content_view));
}

}  // namespace autofill

BEGIN_METADATA(autofill, PaymentsChurnedUsersBubbleView)
END_METADATA
