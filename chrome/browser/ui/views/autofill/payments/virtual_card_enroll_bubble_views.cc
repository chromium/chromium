// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"

#include <memory>

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
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

  const LegalMessageLines message_lines =
      controller_->GetVirtualCardEnrollmentFields()->legal_message_lines;
  if (!message_lines.empty()) {
    legal_message_view_ = SetFootnoteView(std::make_unique<LegalMessageView>(
        message_lines,
        base::BindRepeating(&VirtualCardEnrollBubbleViews::LegalMessageClicked,
                            base::Unretained(this))));
    legal_message_view_->SetID(DialogViewId::FOOTNOTE_VIEW);
  }
}

void VirtualCardEnrollBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void VirtualCardEnrollBubbleViews::Hide() {
  CloseBubble();
  if (controller_)
    controller_->OnBubbleClosed(closed_reason_);
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
  GetBubbleFrameView()->SetTitleView(
      std::make_unique<TitleWithIconAndSeparatorView>(
          GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY));
}

std::u16string VirtualCardEnrollBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void VirtualCardEnrollBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(closed_reason_);
    controller_ = nullptr;
  }
}

void VirtualCardEnrollBubbleViews::OnWidgetClosing(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  closed_reason_ = GetPaymentsBubbleClosedReasonFromWidgetClosedReason(
      widget->closed_reason());
}

VirtualCardEnrollBubbleViews::~VirtualCardEnrollBubbleViews() = default;

std::unique_ptr<views::View>
VirtualCardEnrollBubbleViews::CreateMainContentView() {
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();

  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL));

  // Add main image
  auto* const main_image =
      view->AddChildView(std::make_unique<views::ImageView>());
  main_image->SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          GetNativeTheme()->ShouldUseDarkColors()
              ? IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_DIALOG_DARK
              : IDR_AUTOFILL_VIRTUAL_CARD_ENROLL_DIALOG));

  // Add the card network icon, 'Virtual card', and obfuscated last four digits.
  auto* description_view =
      view->AddChildView(std::make_unique<views::BoxLayoutView>());
  description_view->SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));

  const VirtualCardEnrollmentFields* virtual_card_enrollment_fields =
      controller_->GetVirtualCardEnrollmentFields();
  CreditCard* card = virtual_card_enrollment_fields->credit_card.get();
  gfx::Image* card_image = virtual_card_enrollment_fields->card_art_image.get();

  auto* const card_network_icon =
      description_view->AddChildView(std::make_unique<views::ImageView>());
  card_network_icon->SetImage(card_image->AsImageSkia());
  card_network_icon->SetTooltipText(card->NetworkForDisplay());

  const std::u16string card_info =
      card->CardIdentifierStringForAutofillDisplay();

  const std::u16string card_label_text =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX) +
      u"\n" +
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX_TWO) +
      u" " + card_info;

  auto* const card_identifier_label =
      description_view->AddChildView(std::make_unique<views::StyledLabel>());
  card_identifier_label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  card_identifier_label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  card_identifier_label->SetText(card_label_text);

  uint32_t length =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_VIRTUAL_CARD_ENTRY_PREFIX_TWO)
          .length() +
      card_info.length();
  uint32_t offset = card_label_text.length() - length;

  views::StyledLabel::RangeStyleInfo linked_styling;
  linked_styling.text_style = views::style::STYLE_SECONDARY;
  card_identifier_label->AddStyleRange(gfx::Range(offset, offset + length),
                                       linked_styling);

  // If applicable, add the explanation label.  Appears below the card
  // info.
  std::u16string explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* const explanation_label =
        view->AddChildView(std::make_unique<views::StyledLabel>());
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    explanation_label->SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL);
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
  return view;
}

void VirtualCardEnrollBubbleViews::Init() {
  AddChildView(CreateMainContentView());
}

void VirtualCardEnrollBubbleViews::LearnMoreLinkClicked() {
  if (controller()) {
    controller()->OnLinkClicked(
        autofill::payments::GetVirtualCardEnrollmentSupportUrl());
  }
}

void VirtualCardEnrollBubbleViews::LegalMessageClicked(const GURL& url) {
  if (controller())
    controller()->OnLinkClicked(url);
}

}  // namespace autofill
