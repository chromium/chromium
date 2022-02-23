// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"

class Browser;
class Profile;

// Delegate of the bookmark bubble to load the sign in page in a browser
// when the sign in link is clicked.
class BookmarkBubbleSignInDelegate : public BubbleSyncPromoDelegate,
                                     public BrowserListObserver {
 public:
  explicit BookmarkBubbleSignInDelegate(Browser* browser);

  BookmarkBubbleSignInDelegate(const BookmarkBubbleSignInDelegate&) = delete;
  BookmarkBubbleSignInDelegate& operator=(const BookmarkBubbleSignInDelegate&) =
      delete;

  ~BookmarkBubbleSignInDelegate() override;

  // BubbleSyncPromoDelegate:
  void OnEnableSync(const AccountInfo& account) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 private:
  // Makes sure |browser_| points to a valid browser.
  void EnsureBrowser();

  // The browser in which the sign in page must be loaded.
  raw_ptr<Browser> browser_;

  // The profile associated with |browser_|.
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_
