// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_SIGN_IN_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_SIGN_IN_PROMO_BUBBLE_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace bookmarks {
class BookmarkNode;
}

DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkSigninPromoFrameViewId);

class BookmarkSigninPromoBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(BookmarkSigninPromoBubbleView, LocationBarBubbleDelegateView)

 public:
  BookmarkSigninPromoBubbleView(View* anchor_view,
                                content::WebContents* web_contents,
                                const bookmarks::BookmarkNode* bookmark);
  ~BookmarkSigninPromoBubbleView() override;

  void AddedToWidget() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_SIGN_IN_PROMO_BUBBLE_VIEW_H_
