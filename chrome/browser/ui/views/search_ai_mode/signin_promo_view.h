// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/interaction/element_identifier.h"

namespace views {
class View;
}
class SearchAIModeSignInPromoController;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSearchAIModeSignInPromoFrameViewId);

class SearchAIModeSignInPromoView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(SearchAIModeSignInPromoView, LocationBarBubbleDelegateView)
 public:
  SearchAIModeSignInPromoView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      base::WeakPtr<SearchAIModeSignInPromoController> controller);
  SearchAIModeSignInPromoView(const SearchAIModeSignInPromoView&) = delete;
  SearchAIModeSignInPromoView& operator=(const SearchAIModeSignInPromoView&) =
      delete;

  ~SearchAIModeSignInPromoView() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;

 private:
  base::WeakPtr<SearchAIModeSignInPromoController> controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_AI_MODE_SIGNIN_PROMO_VIEW_H_
