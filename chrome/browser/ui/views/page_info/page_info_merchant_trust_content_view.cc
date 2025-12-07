// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"

#include <string>

#include "base/i18n/message_formatter.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/star_rating_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoMerchantTrustContentView,
                                      kElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoMerchantTrustContentView,
                                      kViewReviewsId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoMerchantTrustContentView,
                                      kHatsButtonId);

PageInfoMerchantTrustContentView::PageInfoMerchantTrustContentView() {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  // The distance between the last button and the footer.
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  SetProperty(views::kElementIdentifierKey, kElementIdForTesting);
  SetOrientation(views::LayoutOrientation::kVertical);

  AddChildView(CreateDescriptionLabel());
  AddChildView(CreateReviewsSummarySection());
  view_reviews_button_ = AddChildView(CreateViewReviewsButton());
  view_reviews_button_->SetProperty(views::kMarginsKey,
                                    gfx::Insets().set_bottom(bottom_margin));
  hats_button_ = AddChildView(CreateHatsButton());
  // No bottom margin for the content view because the HaTS button acts as a
  // footer.
  SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 0));
}

PageInfoMerchantTrustContentView::~PageInfoMerchantTrustContentView() = default;

base::CallbackListSubscription
PageInfoMerchantTrustContentView::RegisterLearnMoreLinkPressedCallback(
    base::RepeatingCallback<void(const ui::Event&)> callback) {
  return learn_more_link_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
PageInfoMerchantTrustContentView::RegisterViewReviewsButtonPressedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return view_reviews_button_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
PageInfoMerchantTrustContentView::RegisterHatsButtonPressedCallback(
    base::RepeatingClosureList::CallbackType callback) {
  return hats_button_callback_list_.Add(std::move(callback));
}

void PageInfoMerchantTrustContentView::SetReviewsSummary(
    std::u16string summary) {
  // TODO(crbug.com/378854730): Consider hiding the summary and description if
  // |summary| is empty.
  summary_label_->SetText(summary);
}

void PageInfoMerchantTrustContentView::SetRatingAndReviewCount(double rating,
                                                               int count) {
  star_rating_view_->SetRating(rating);

  view_reviews_button_->SetTitleText(l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_MERCHANT_TRUST_VIEW_ALL_REVIEWS, count));
  auto a11y_description = base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_MERCHANT_TRUST_STAR_RATING_AND_COUNT_A11Y_DESCRIPTION),
      count, rating);
  view_reviews_button_->GetViewAccessibility().SetName(a11y_description);
}

void PageInfoMerchantTrustContentView::SetHatsButtonVisibility(bool visible) {
  hats_button_->SetVisible(visible);
}

void PageInfoMerchantTrustContentView::SetHatsButtonTitleId(int title_id) {
  hats_button_->SetTitleText(l10n_util::GetStringUTF16(title_id));
}

std::unique_ptr<views::View>
PageInfoMerchantTrustContentView::CreateDescriptionLabel() {
  auto description_label = std::make_unique<views::StyledLabel>();

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  // The top and bottom margins should be the same as for buttons shown below.
  const auto button_insets = layout_provider->GetInsetsMetric(
      ChromeInsetsMetric::INSETS_PAGE_INFO_HOVER_BUTTON);

  description_label->SetProperty(views::kMarginsKey, button_insets);
  description_label->SetDefaultTextStyle(views::style::STYLE_BODY_4);
  description_label->SetDefaultEnabledColorId(kColorPageInfoSubtitleForeground);
  description_label->SizeToFit(PageInfoViewFactory::kMinBubbleWidth -
                               button_insets.width());

  description_label->SetText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_DESCRIPTION));

  return description_label;
}

std::unique_ptr<views::View>
PageInfoMerchantTrustContentView::CreateReviewsSummarySection() {
  auto container = std::make_unique<views::FlexLayoutView>();
  container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  // Use the same insets as buttons and permission rows in the main page for
  // consistency.
  container->SetInteriorMargin(ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_PAGE_INFO_HOVER_BUTTON));

  auto* icon =
      container->AddChildView(std::make_unique<NonAccessibleImageView>());
  icon->SetImage(
      PageInfoViewFactory::GetImageModel(vector_icons::kChatSparkIcon));

  auto* labels_wrapper =
      container->AddChildView(PageInfoViewFactory::CreateLabelWrapper());
  auto* title = labels_wrapper->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_MERCHANT_TRUST_REVIEWS_SUMMARY_TITLE),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  title->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  summary_label_ = labels_wrapper->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_4));
  summary_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  summary_label_->SetMultiLine(true);
  summary_label_->SetEnabledColor(kColorPageInfoSubtitleForeground);

  return container;
}

std::unique_ptr<RichHoverButton>
PageInfoMerchantTrustContentView::CreateViewReviewsButton() {
  auto merchant_trust_button = std::make_unique<RichHoverButton>(
      base::BindRepeating(
          &PageInfoMerchantTrustContentView::NotifyViewReviewsPressed,
          base::Unretained(this)),
      PageInfoViewFactory::GetImageModel(vector_icons::kChatIcon),
      std::u16string(), std::u16string(), PageInfoViewFactory::GetLaunchIcon());
  merchant_trust_button->SetTitleTextStyleAndColor(
      views::style::STYLE_BODY_3_MEDIUM, kColorPageInfoForeground);
  merchant_trust_button->SetProperty(views::kElementIdentifierKey,
                                     kViewReviewsId);
  star_rating_view_ =
      merchant_trust_button->SetCustomView(std::make_unique<StarRatingView>());
  return merchant_trust_button;
}

std::unique_ptr<RichHoverButton>
PageInfoMerchantTrustContentView::CreateHatsButton() {
  auto hats_button = std::make_unique<RichHoverButton>(
      base::BindRepeating(
          &PageInfoMerchantTrustContentView::NotifyHatsButtonPressed,
          base::Unretained(this)),
      PageInfoViewFactory::GetImageModel(kSubmitFeedbackIcon),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HATS_BUTTON),
      std::u16string());
  hats_button->SetBackground(
      views::CreateSolidBackground(ui::kColorSysNeutralContainer));
  hats_button->SetProperty(views::kElementIdentifierKey, kHatsButtonId);
  hats_button->SetVisible(false);
  hats_button->SetBorder(
      views::CreateEmptyBorder(ChromeLayoutProvider::Get()->GetInsetsMetric(
          INSETS_PAGE_INFO_FOOTER_BUTTON)));
  return hats_button;
}

void PageInfoMerchantTrustContentView::NotifyLearnMoreLinkPressed(
    const ui::Event& event) {
  learn_more_link_callback_list_.Notify(event);
}

void PageInfoMerchantTrustContentView::NotifyViewReviewsPressed() {
  view_reviews_button_callback_list_.Notify();
}

void PageInfoMerchantTrustContentView::NotifyHatsButtonPressed() {
  hats_button_callback_list_.Notify();
}

gfx::Size PageInfoMerchantTrustContentView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Size the content view based on the "View all reviews button", since the
  // description and summary labels are multiline and shouldn't be used to
  // size the view.
  const int width = std::max(PageInfoViewFactory::kMinBubbleWidth,
                             view_reviews_button_->GetPreferredSize().width());
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}
