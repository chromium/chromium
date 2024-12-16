// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/star_rating_view.h"
#include "ui/views/layout/flex_layout_view.h"

class StarRatingView;
class RichHoverButton;

// The view that is used as a content view of the "Merchant trust" subpage
// in page info.
class PageInfoMerchantTrustContentView : public views::FlexLayoutView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementIdForTesting);

  PageInfoMerchantTrustContentView();
  ~PageInfoMerchantTrustContentView() override;

  void SetRating(double rating);
  void SetReviewCount(int count);

 private:
  [[nodiscard]] std::unique_ptr<views::View> CreateDescriptionLabel();
  [[nodiscard]] std::unique_ptr<RichHoverButton> CreateViewReviewsButton();

  void LearnMoreLinkClicked(const ui::Event& event);
  void OpenReviewsInSidePanel();

  raw_ptr<StarRatingView> star_rating_view_;
  raw_ptr<RichHoverButton> view_reviews_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MERCHANT_TRUST_CONTENT_VIEW_H_
