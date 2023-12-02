// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_button.h"
#include "chrome/browser/ui/views/autofill/payments/promo_code_label_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/controls/subpage_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"

namespace autofill {
DEFINE_ELEMENT_IDENTIFIER_VALUE(kOfferNotificationBubbleElementId);

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
  SetProperty(views::kElementIdentifierKey, kOfferNotificationBubbleElementId);
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
      InitWithGPayPromoCodeOfferContent();
      return;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      InitWithFreeListingCouponOfferContent();
      return;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED_NORETURN();
  }
}

void OfferNotificationBubbleViews::AddedToWidget() {
  const AutofillOfferData* offer = controller_->GetOffer();
  CHECK(offer);

  if (offer->IsFreeListingCouponOffer()) {
    GetBubbleFrameView()->SetTitleView(
        CreateFreeListingCouponOfferMainPageTitleView(*offer));
    GetBubbleFrameView()->SetHeaderView(
        CreateFreeListingCouponOfferMainPageHeaderView());
  } else {
    GetBubbleFrameView()->SetTitleView(CreateTitleView(
        GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_G));
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
  if (!widget->IsClosed()) {
    return;
  }
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

  auto dialog_insets =
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText);
  if (::features::IsChromeRefresh2023()) {
    dialog_insets = ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
        views::DialogContentType::kControl, views::DialogContentType::kText);
  }
  set_margins(
      gfx::Insets::TLBR(dialog_insets.top(), 0, dialog_insets.bottom(), 0));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  GURL url = web_contents()->GetLastCommittedURL();
  std::string seller_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  free_listing_coupon_page_container_ =
      AddChildView(std::make_unique<PageSwitcherView>(
          CreateFreeListingCouponOfferMainPageContent(
              *(controller_->GetOffer()), seller_domain)));
}

std::unique_ptr<views::View>
OfferNotificationBubbleViews::CreateFreeListingCouponOfferMainPageContent(
    const AutofillOfferData& offer,
    const std::string& seller_domain) {
  CHECK(!offer.GetPromoCode().empty());

  auto main_page_view = std::make_unique<views::View>();

  // Create bubble content:
  if (::features::IsChromeRefresh2023()) {
    gfx::Insets dialog_insets =
        ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
            views::DialogContentType::kControl,
            views::DialogContentType::kText);

    auto* layout =
        main_page_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical,
            gfx::Insets::TLBR(0, dialog_insets.left(), 0,
                              dialog_insets.right()),
            ChromeLayoutProvider::Get()->GetDistanceMetric(
                views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
  } else {
    auto dialog_insets =
        ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
            views::DialogContentType::kText, views::DialogContentType::kText);

    auto* layout =
        main_page_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetIgnoreDefaultMainAxisMargins(true)
        .SetCollapseMargins(true)
        .SetDefault(
            views::kMarginsKey,
            gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                            dialog_insets.left()))
        .SetDefault(
            views::kFlexBehaviorKey,
            views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                     views::MaximumFlexSizeRule::kPreferred,
                                     /*adjust_height_for_width*/ true));
  }

  std::u16string promo_code_value_prop_string;

  if (::features::IsChromeRefresh2023()) {
    const int dialog_inset = views::LayoutProvider::Get()
                                 ->GetInsetsMetric(views::INSETS_DIALOG)
                                 .left();
    const int dialog_width = views::LayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    auto promo_code_label_view_preferred_size =
        gfx::Size(dialog_width - dialog_inset * 2, 0);
    promo_code_label_view_ =
        main_page_view->AddChildView(std::make_unique<PromoCodeLabelView>(
            promo_code_label_view_preferred_size,
            base::ASCIIToUTF16(offer.GetPromoCode()),
            base::BindRepeating(
                &OfferNotificationBubbleViews::OnPromoCodeButtonClicked,
                base::Unretained(this))));
    promo_code_value_prop_string = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_PROMO_CODE_OFFERS_USE_THIS_CODE_TEXT);
  } else {
    promo_code_label_button_ =
        main_page_view->AddChildView(std::make_unique<PromoCodeLabelButton>(
            base::BindRepeating(
                &OfferNotificationBubbleViews::OnPromoCodeButtonClicked,
                base::Unretained(this)),
            base::ASCIIToUTF16(offer.GetPromoCode())));
    if (!offer.GetDisplayStrings().value_prop_text.empty()) {
      promo_code_value_prop_string =
          base::ASCIIToUTF16(offer.GetDisplayStrings().value_prop_text);
    }
  }

  if (base::FeatureList::IsEnabled(commerce::kShowDiscountOnNavigation)) {
    auto expiration_date_text = l10n_util::GetStringFUTF16(
        IDS_DISCOUNT_EXPIRATION_DATE, TimeFormatShortDate(offer.GetExpiry()));
    if (promo_code_value_prop_string.empty()) {
      promo_code_value_prop_string = expiration_date_text;
    } else {
      promo_code_value_prop_string = l10n_util::GetStringFUTF16(
          IDS_TWO_STRINGS_CONNECTOR, promo_code_value_prop_string,
          expiration_date_text);
    }
  }

  if (!promo_code_value_prop_string.empty()) {
    promo_code_value_prop_label_ = main_page_view->AddChildView(
        views::Builder<views::StyledLabel>()
            .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .Build());

    if (offer.GetTermsAndConditions().has_value() &&
        !offer.GetTermsAndConditions().value().empty()) {
      std::vector<size_t> offsets;
      promo_code_value_prop_string = l10n_util::GetStringFUTF16(
          IDS_TWO_STRINGS_CONNECTOR_WITH_SPACE, promo_code_value_prop_string,
          l10n_util::GetStringUTF16(IDS_SEE_SELLER_TERMS_AND_CONDITIONS),
          &offsets);
      promo_code_value_prop_label_->SetText(promo_code_value_prop_string);
      size_t terms_and_conditions_offset = offsets[1];
      base::RepeatingCallback<void()> callback = base::BindRepeating(
          &OfferNotificationBubbleViews::OpenTermsAndConditionsPage,
          weak_factory_.GetWeakPtr(), offer, seller_domain);
      views::StyledLabel::RangeStyleInfo terms_and_conditions_style_info =
          views::StyledLabel::RangeStyleInfo::CreateForLink(
              std::move(callback));
      promo_code_value_prop_label_->AddStyleRange(
          gfx::Range(terms_and_conditions_offset,
                     promo_code_value_prop_string.length()),
          terms_and_conditions_style_info);
    } else {
      promo_code_value_prop_label_->SetText(promo_code_value_prop_string);
    }

    if (!::features::IsChromeRefresh2023()) {
      promo_code_value_prop_label_->SetProperty(views::kCrossAxisAlignmentKey,
                                                views::LayoutAlignment::kStart);
    }
  }

  UpdateButtonTooltipsAndAccessibleNames();
  return main_page_view;
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
  std::u16string tooltip = controller_->GetPromoCodeButtonTooltip();

  if (promo_code_label_button_) {
    promo_code_label_button_->SetTooltipText(tooltip);
    promo_code_label_button_->SetAccessibleName(
        base::StrCat({promo_code_label_button_->GetText(), u" ", tooltip}));
  } else if (promo_code_label_view_) {
    promo_code_label_view_->UpdateCopyButtonTooltipsAndAccessibleNames(tooltip);
  }
}

void OfferNotificationBubbleViews::OpenTermsAndConditionsPage(
    AutofillOfferData offer,
    std::string seller_domain) {
  ResetPointersToFreeListingCouponOfferMainPageContent();

  auto footer_message =
      l10n_util::GetStringFUTF16(IDS_SELLER_TERMS_AND_CONDITIONS_DIALOG_FOOTER,
                                 base::ASCIIToUTF16(seller_domain));
  free_listing_coupon_page_container_->SwitchToPage(
      views::Builder<SubpageView>(
          std::make_unique<SubpageView>(
              base::BindRepeating(&OfferNotificationBubbleViews::
                                      OpenFreeListingCouponOfferMainPage,
                                  weak_factory_.GetWeakPtr(), offer,
                                  seller_domain),
              GetBubbleFrameView()))
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_SELLER_TERMS_AND_CONDITIONS_DIALOG_TITLE))
          .SetContentView(
              views::Builder<views::Label>()
                  .SetText(
                      base::ASCIIToUTF16(offer.GetTermsAndConditions().value()))
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .Build())
          .SetHeaderView(nullptr)
          .SetFootnoteView(
              // TODO(b:289240484): Need to elide the front of the seller_domain
              // if needed.
              views::Builder<views::Label>()
                  .SetText(footer_message)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SetMaxLines(2)
                  .SetAllowCharacterBreak(true)
                  .Build())
          .Build());
  SizeToContents();
}

std::unique_ptr<views::View>
OfferNotificationBubbleViews::CreateFreeListingCouponOfferMainPageHeaderView() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  return std::make_unique<ThemeTrackingNonAccessibleImageView>(
      ::features::IsChromeRefresh2023()
          ? *bundle.GetImageSkiaNamed(
                IDR_AUTOFILL_OFFERS_LIGHT_CHROME_REFRESH_2023)
          : *bundle.GetImageSkiaNamed(IDR_AUTOFILL_OFFERS),
      ::features::IsChromeRefresh2023()
          ? *bundle.GetImageSkiaNamed(
                IDR_AUTOFILL_OFFERS_DARK_CHROME_REFRESH_2023)
          : *bundle.GetImageSkiaNamed(IDR_AUTOFILL_OFFERS),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
}

std::unique_ptr<views::View>
OfferNotificationBubbleViews::CreateFreeListingCouponOfferMainPageTitleView(
    const AutofillOfferData& offer) {
  if (::features::IsChromeRefresh2023()) {
    auto title_label = std::make_unique<views::Label>(
        base::ASCIIToUTF16(offer.GetDisplayStrings().value_prop_text),
        views::style::CONTEXT_DIALOG_TITLE);
    title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_label->SetMultiLine(true);
    return title_label;
  }

  return std::make_unique<TitleWithIconAndSeparatorView>(
      GetWindowTitle(), TitleWithIconAndSeparatorView::Icon::GOOGLE_G);
}

void OfferNotificationBubbleViews::OpenFreeListingCouponOfferMainPage(
    AutofillOfferData offer,
    std::string seller_domain) {
  GetBubbleFrameView()->SetHeaderView(
      CreateFreeListingCouponOfferMainPageHeaderView());
  GetBubbleFrameView()->SetTitleView(
      CreateFreeListingCouponOfferMainPageTitleView(offer));
  GetBubbleFrameView()->SetFootnoteView(nullptr);
  free_listing_coupon_page_container_->SwitchToPage(
      CreateFreeListingCouponOfferMainPageContent(offer, seller_domain));
  SizeToContents();
}

void OfferNotificationBubbleViews::
    ResetPointersToFreeListingCouponOfferMainPageContent() {
  promo_code_value_prop_label_ = nullptr;
  promo_code_label_button_ = nullptr;
  promo_code_label_view_ = nullptr;
}

}  // namespace autofill
