// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include "build/buildflag.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/signin/signin_ui_util.h"

BookmarkBubbleSignInDelegate::BookmarkBubbleSignInDelegate(Browser* browser)
    : browser_(browser), profile_(browser->profile()->GetOriginalProfile()) {
  if (profile_ != browser_->profile())
    browser_ = nullptr;

  BrowserList::AddObserver(this);
}

BookmarkBubbleSignInDelegate::~BookmarkBubbleSignInDelegate() {
  BrowserList::RemoveObserver(this);
}

void BookmarkBubbleSignInDelegate::OnEnableSync(const AccountInfo& account) {
  EnsureBrowser();
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      browser_, account,
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE);

  // TODO(msarda): Close the bookmarks bubble once the enable sync flow has
  // started.
}

void BookmarkBubbleSignInDelegate::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_)
    browser_ = nullptr;
}

void BookmarkBubbleSignInDelegate::EnsureBrowser() {
  if (!browser_) {
    browser_ = chrome::FindLastActiveWithProfile(profile_);
    if (!browser_)
      browser_ = Browser::Create(Browser::CreateParams(profile_, true));
  }
}
