// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"

namespace autofill {

OfferNotificationBubbleViews::OfferNotificationBubbleViews(
    views::View* anchor_view,
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller) {
  DCHECK(controller);
  SetShowCloseButton(true);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
}

OfferNotificationBubbleViews::~OfferNotificationBubbleViews() {
  Hide();
}

void OfferNotificationBubbleViews::Hide() {
  CloseBubble();
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
  }
  controller_ = nullptr;
}

void OfferNotificationBubbleViews::Init() {
  switch (controller_->GetOffer()->GetOfferType()) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      InitWithCardLinkedOfferContent();
      return;
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      if (base::FeatureList::IsEnabled(
              features::kAutofillFillMerchantPromoCodeFields)) {
        InitWithGPayPromoCodeOfferContent();
      } else {
        InitWithFreeListingCouponOfferContent();
      }
      return;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      InitWithFreeListingCouponOfferContent();
      return;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED_NORETURN();
  }
}

void OfferNotificationBubbleViews::AddedToWidget() {
  GetBubbleFrameView()->SetTitleView(CreateTitleView(
      GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_G));

  // Set the header image for free listing coupon notification bubble.
  // If the promo code field autofill feature is off, we also need to add this
  // image for GPay promo code offer because we are showing the free listing
  // coupon bubble in this case.
  if (controller_->GetOffer()->IsFreeListingCouponOffer() ||
      (!base::FeatureList::IsEnabled(
           features::kAutofillFillMerchantPromoCodeFields) &&
       controller_->GetOffer()->IsGPayPromoCodeOffer())) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    auto* autofill_offers_banner =
        bundle.GetImageSkiaNamed(IDR_AUTOFILL_OFFERS);
    auto image_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
        *autofill_offers_banner, *autofill_offers_banner,
        base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                            base::Unretained(this)));
    GetBubbleFrameView()->SetHeaderView(std::move(image_view));
  }
}

std::u16string OfferNotificationBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : std::u16string();
}

void OfferNotificationBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        GetPaymentsBubbleClosedReasonFromWidget(GetWidget()));
    controller_ = nullptr;
  }
}

void OfferNotificationBubbleViews::OnWidgetDestroying(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetDestroying(widget);
  if (!widget->IsClosed())
    return;
  DCHECK_NE(widget->closed_reason(),
            views::Widget::ClosedReason::kCancelButtonClicked);
}

void OfferNotificationBubbleViews::InitWithCardLinkedOfferContent() {
  // Card-linked offers have a positive CTA button:
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, controller_->GetOkButtonLabel());

  // Create bubble content:
  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  auto* explanatory_message = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_DESCRIPTION_TEXT,
          controller_->GetLinkedCard()->CardNameAndLastFourDigits()),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
  explanatory_message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  explanatory_message->SetMultiLine(true);
}

void OfferNotificationBubbleViews::InitWithFreeListingCouponOfferContent() {
  // Promo code offers have no CTA buttons:
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // Create bubble content:
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  const AutofillOfferData* offer = controller_->GetOffer();
  DCHECK(offer);
  DCHECK(!offer->GetPromoCode().empty());

  promo_code_label_button_ =
      AddChildView(std::make_unique<PromoCodeLabelButton>(
          base::BindRepeating(
              &OfferNotificationBubbleViews::OnPromoCodeButtonClicked,
              base::Unretained(this)),
          base::ASCIIToUTF16(offer->GetPromoCode())));

  if (!offer->GetDisplayStrings().value_prop_text.empty()) {
    auto* promo_code_value_prop = AddChildView(std::make_unique<views::Label>(
        base::ASCIIToUTF16(offer->GetDisplayStrings().value_prop_text),
        views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY));
    promo_code_value_prop->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    promo_code_value_prop->SetMultiLine(true);
  }
  UpdateButtonTooltipsAndAccessibleNames();
}

void OfferNotificationBubbleViews::InitWithGPayPromoCodeOfferContent() {
  // Promo code offers have no CTA buttons:
  SetButtons(ui::DIALOG_BUTTON_NONE);

  // Create bubble content:
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  const AutofillOfferData* offer = controller_->GetOffer();
  DCHECK(offer);
  const std::string& value_prop_text =
      offer->GetDisplayStrings().value_prop_text;
  // Add the first line of `value_prop_text` with see details link.
  if (!value_prop_text.empty()) {
    // Hide See details if the link is not valid.
    const std::string& see_details_text =
        offer->GetOfferDetailsUrl().is_valid()
            ? offer->GetDisplayStrings().see_details_text
            : "";
    promo_code_label_ = AddChildView(std::make_unique<views::StyledLabel>());
    std::vector<size_t> offsets;
    auto label_text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_GPAY_PROMO_CODE_OFFERS_REMINDER_VALUE_PROP_TEXT,
        base::UTF8ToUTF16(value_prop_text), base::UTF8ToUTF16(see_details_text),
        &offsets);
    promo_code_label_->SetText(label_text);
    promo_code_label_->SetTextContext(
        ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL);
    promo_code_label_->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
    promo_code_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!see_details_text.empty()) {
      promo_code_label_->AddStyleRange(
          gfx::Range(offsets.at(1), offsets.at(1) + see_details_text.length()),
          views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
              &OfferNotificationBubbleViews::OnPromoCodeSeeDetailsClicked,
              base::Unretained(this))));
    }
  }
  // Add the usage instructions text.
  if (!offer->GetDisplayStrings().usage_instructions_text.empty()) {
    instructions_label_ = AddChildView(std::make_unique<views::Label>(
        base::ASCIIToUTF16(offer->GetDisplayStrings().usage_instructions_text),
        ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY));
    instructions_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    instructions_label_->SetMultiLine(true);
  }
}

void OfferNotificationBubbleViews::OnPromoCodeButtonClicked() {
  // Copy clicked promo code to clipboard.
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::ASCIIToUTF16(controller_->GetOffer()->GetPromoCode()));

  // Update controller and tooltip.
  controller_->OnPromoCodeButtonClicked();
  UpdateButtonTooltipsAndAccessibleNames();
}

void OfferNotificationBubbleViews::OnPromoCodeSeeDetailsClicked() {
  DCHECK(controller_->GetOffer()->GetOfferDetailsUrl().is_valid());
  web_contents()->OpenURL(content::OpenURLParams(
      GURL(controller_->GetOffer()->GetOfferDetailsUrl()), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false));
}

void OfferNotificationBubbleViews::UpdateButtonTooltipsAndAccessibleNames() {
  if (!promo_code_label_button_)
    return;

  std::u16string tooltip = controller_->GetPromoCodeButtonTooltip();
  promo_code_label_button_->SetTooltipText(tooltip);
  promo_code_label_button_->SetAccessibleName(
      base::StrCat({promo_code_label_button_->GetText(), u" ", tooltip}));
}

}  // namespace autofill
