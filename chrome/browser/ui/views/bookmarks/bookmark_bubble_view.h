// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_

#include <memory>

#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"
#include "ui/base/interaction/element_identifier.h"

class GURL;
class Browser;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Image;
}

namespace image_fetcher {
struct RequestMetadata;
}

namespace views {
class BubbleDialogDelegate;
class Button;
class View;
}

DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkBubbleOkButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkFolderFieldId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkNameFieldId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkSecondaryButtonId);

// BookmarkBubbleView provides a dialog for unstarring and editing the bookmark
// it is created with. The dialog is created using the static ShowBubble method.
class BookmarkBubbleView {
 public:
  BookmarkBubbleView(const BookmarkBubbleView&) = delete;
  BookmarkBubbleView& operator=(const BookmarkBubbleView&) = delete;

  static void ShowBubble(views::View* anchor_view,
                         content::WebContents* web_contents,
                         views::Button* highlighted_button,
                         std::unique_ptr<BubbleSignInPromoDelegate> delegate,
                         Browser* browser,
                         const GURL& url,
                         bool already_bookmarked);

  static void Hide();

  static void HandleImageUrlResponse(const Profile* profile,
                                     const GURL& image_service_url);

  static void HandleImageBytesResponse(
      const gfx::Image& image,
      const image_fetcher::RequestMetadata& metadata);

  static views::BubbleDialogDelegate* bookmark_bubble() {
    return bookmark_bubble_;
  }

 private:
  class BookmarkBubbleDelegate;
  // The bookmark bubble, if we're showing one.
  static views::BubbleDialogDelegate* bookmark_bubble_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_H_
