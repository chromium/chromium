// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_

#include <memory>

#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class BubbleDialogDelegate;
class Button;
class View;
}

// BookmarkBubbleView provides a dialog for unstarring and editing the bookmark
// it is created with. The dialog is created using the static ShowBubble method.
class BookmarkBubbleView {
 public:
  BookmarkBubbleView(const BookmarkBubbleView&) = delete;
  BookmarkBubbleView& operator=(const BookmarkBubbleView&) = delete;

  static void ShowBubble(views::View* anchor_view,
                         content::WebContents* web_contents,
                         views::Button* highlighted_button,
                         std::unique_ptr<BubbleSyncPromoDelegate> delegate,
                         Profile* profile,
                         const GURL& url,
                         bool already_bookmarked);

  static void Hide();

  static views::BubbleDialogDelegate* bookmark_bubble() {
    return bookmark_bubble_;
  }

 private:
  class BookmarkBubbleDelegate;
  // The bookmark bubble, if we're showing one.
  static views::BubbleDialogDelegate* bookmark_bubble_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
