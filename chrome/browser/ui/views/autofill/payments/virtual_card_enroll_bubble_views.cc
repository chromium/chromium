// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"

#include <memory>

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/ui/payments/virtual_card_enroll_bubble_controller.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "url/gurl.h"

namespace autofill {

VirtualCardEnrollBubbleViews::VirtualCardEnrollBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller->GetAcceptButtonText());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, controller->GetDeclineButtonText());
  SetCancelCallback(base::BindOnce(
      &VirtualCardEnrollBubbleViews::OnDialogDeclined, base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(
      &VirtualCardEnrollBubbleViews::OnDialogAccepted, base::Unretained(this)));

  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  raw_ptr<views::View> legal_message_view =
      SetFootnoteView(CreateLegalMessageView());
  legal_message_view->SetID(DialogViewId::FOOTNOTE_VIEW);
}

VirtualCardEnrollBubbleViews::~VirtualCardEnrollBubbleViews() = default;

void VirtualCardEnrollBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void VirtualCardEnrollBubbleViews::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void VirtualCardEnrollBubbleViews::OnDialogAccepted() {
  if (controller_)
    controller_->OnAcceptButton();
}

void VirtualCardEnrollBubbleViews::OnDialogDeclined() {
  if (controller_)
    controller_->OnDeclineButton();
}

void VirtualCardEnrollBubbleViews::AddedToWidget() {
  auto header_view = std::make_unique<views::BoxLayoutView>();
  header_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  header_view->SetInsideBorderInsets(ChromeLayoutProvider::Get()
                                         ->GetInsetsMetric(views::INSETS_DIALOG)
                                         .set_bottom(0));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_DIALOG),
      *bundle.GetImageSkiaNamed(IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_DIALOG_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));

  header_view->AddChildView(std::move(image_view));

  GetBubbleFrameView()->SetHeaderView(std::move(header_view));
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

std::u16string VirtualCardEnrollBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void VirtualCardEnrollBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void VirtualCardEnrollBubbleViews::Init() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // If applicable, add the explanation label.  Appears above the card
  // info.
  std::u16string explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* const explanation_label =
        AddChildView(std::make_unique<views::StyledLabel>());
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    explanation_label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
    explanation_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
    explanation_label->SetText(explanation);

    views::StyledLabel::RangeStyleInfo style_info =
        views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
            &VirtualCardEnrollBubbleViews::LearnMoreLinkClicked,
            weak_ptr_factory_.GetWeakPtr()));

    uint32_t offset =
        explanation.length() - controller_->GetLearnMoreLinkText().length();
    explanation_label->AddStyleRange(
        gfx::Range(offset,
                   offset + controller_->GetLearnMoreLinkText().length()),
        style_info);
  }

  // Add the card network icon, 'Virtual card', and obfuscated last four digits.
  auto* description_view =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  description_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  description_view->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  const VirtualCardEnrollmentFields virtual_card_enrollment_fields =
      controller_->GetVirtualCardEnrollmentFields();

  CreditCard card = virtual_card_enrollment_fields.credit_card;

  auto* card_image =
      description_view->AddChildView(std::make_unique<views::ImageView>());
  card_image->SetImage(virtual_card_enrollment_fields.card_art_image);
  card_image->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_CARD_IMAGE_TOOLTIP));

  auto* const card_identifier_view =
      description_view->AddChildView(std::make_unique<views::BoxLayoutView>());
  card_identifier_view->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  card_identifier_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  auto* card_name_4digits_view = card_identifier_view->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  card_name_4digits_view->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  card_name_4digits_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  auto* card_name_label =
      card_name_4digits_view->AddChildView(std::make_unique<views::Label>(
          card.CardNameForAutofillDisplay(),
          views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY));
  card_name_4digits_view->SetFlexForView(card_name_label, /*flex=*/1);
  card_name_4digits_view->AddChildView(std::make_unique<views::Label>(
      card.ObfuscatedLastFourDigits(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_PRIMARY));
  card_identifier_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX),
      ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
      views::style::STYLE_SECONDARY));
}

std::unique_ptr<views::View>
VirtualCardEnrollBubbleViews::CreateLegalMessageView() {
  auto legal_message_view = std::make_unique<views::BoxLayoutView>();
  legal_message_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  legal_message_view->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));

  const LegalMessageLines google_legal_message =
      controller_->GetVirtualCardEnrollmentFields().google_legal_message;
  const LegalMessageLines issuser_legal_message =
      controller_->GetVirtualCardEnrollmentFields().issuer_legal_message;

  DCHECK(!google_legal_message.empty());
  legal_message_view->AddChildView(std::make_unique<LegalMessageView>(
      google_legal_message, /*user_email=*/absl::nullopt,
      /*user_avatar=*/absl::nullopt,
      base::BindRepeating(
          &VirtualCardEnrollBubbleViews::GoogleLegalMessageClicked,
          base::Unretained(this))));

  if (!issuser_legal_message.empty()) {
    legal_message_view->AddChildView(std::make_unique<LegalMessageView>(
        issuser_legal_message, /*user_email=*/absl::nullopt,
        /*user_avatar=*/absl::nullopt,
        base::BindRepeating(
            &VirtualCardEnrollBubbleViews::IssuerLegalMessageClicked,
            base::Unretained(this))));
  }
  return legal_message_view;
}

void VirtualCardEnrollBubbleViews::LearnMoreLinkClicked() {
  if (controller()) {
    controller()->OnLinkClicked(
        VirtualCardEnrollmentLinkType::VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
        autofill::payments::GetVirtualCardEnrollmentSupportUrl());
  }
}

void VirtualCardEnrollBubbleViews::IssuerLegalMessageClicked(const GURL& url) {
  if (controller()) {
    controller()->OnLinkClicked(
        VirtualCardEnrollmentLinkType::VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
        url);
  }
}

void VirtualCardEnrollBubbleViews::GoogleLegalMessageClicked(const GURL& url) {
  if (controller()) {
    controller()->OnLinkClicked(
        VirtualCardEnrollmentLinkType::
            VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK,
        url);
  }
}

}  // namespace autofill
