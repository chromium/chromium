// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/star_rating_view.h"
#include "components/page_info/page_info_ui.h"
#include "ui/views/layout/flex_layout_view.h"

class StarRatingView;
class RichHoverButton;

// The view that is used as a content view of the "Merchant trust" subpage
// in page info.
// TODO(crbug.com/390370438): With current implementation, PageInfo requires an
// active instance of PageInfoUI. `PageInfoMerchantTrustContentView` doesn't
// actually implement any of the methods.
class PageInfoMerchantTrustContentView : public views::FlexLayoutView,
                                         public PageInfoUI {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewReviewsId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHatsButtonId);

  PageInfoMerchantTrustContentView();
  ~PageInfoMerchantTrustContentView() override;

  base::CallbackListSubscription RegisterLearnMoreLinkPressedCallback(
      base::RepeatingCallback<void(const ui::Event&)> callback);
  base::CallbackListSubscription RegisterViewReviewsButtonPressedCallback(
      base::RepeatingClosureList::CallbackType callback);
  base::CallbackListSubscription RegisterHatsButtonPressedCallback(
      base::RepeatingClosureList::CallbackType callback);

  void SetReviewsSummary(std::u16string summary);
  void SetRatingAndReviewCount(double rating, int count);
  void SetHatsButtonVisibility(bool visible);
  void SetHatsButtonTitleId(int title_id);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  [[nodiscard]] std::unique_ptr<views::View> CreateDescriptionLabel();
  [[nodiscard]] std::unique_ptr<views::View> CreateReviewsSummarySection();
  [[nodiscard]] std::unique_ptr<RichHoverButton> CreateViewReviewsButton();
  [[nodiscard]] std::unique_ptr<RichHoverButton> CreateHatsButton();

  void NotifyLearnMoreLinkPressed(const ui::Event& event);
  void NotifyViewReviewsPressed();
  void NotifyHatsButtonPressed();

  raw_ptr<StarRatingView> star_rating_view_;
  raw_ptr<RichHoverButton> view_reviews_button_;
  raw_ptr<views::Label> summary_label_;
  raw_ptr<RichHoverButton> hats_button_;

  base::RepeatingCallbackList<void(const ui::Event&)>
      learn_more_link_callback_list_;
  base::RepeatingClosureList view_reviews_button_callback_list_;
  base::RepeatingClosureList hats_button_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_
