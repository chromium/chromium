// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/signin/bubble_signin_promo_delegate.h"

class Profile;

// Delegate of the bookmark bubble to load the sign in page in a browser
// when the sign in link is clicked.
class BookmarkBubbleSignInDelegate : public BubbleSignInPromoDelegate {
 public:
  explicit BookmarkBubbleSignInDelegate(Profile* profile);

  BookmarkBubbleSignInDelegate(const BookmarkBubbleSignInDelegate&) = delete;
  BookmarkBubbleSignInDelegate& operator=(const BookmarkBubbleSignInDelegate&) =
      delete;

  ~BookmarkBubbleSignInDelegate() override;

  // BubbleSignInPromoDelegate:
  void OnSignIn(const AccountInfo& account) override;

 private:
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_SIGN_IN_DELEGATE_H_
