// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/signin/public/identity_manager/account_info.h"

BookmarkBubbleSignInDelegate::BookmarkBubbleSignInDelegate(Profile* profile)
    : profile_(profile->GetOriginalProfile()) {}

BookmarkBubbleSignInDelegate::~BookmarkBubbleSignInDelegate() = default;

void BookmarkBubbleSignInDelegate::OnSignIn(const AccountInfo& account) {
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      profile_, account,
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE);

  // TODO(msarda): Close the bookmarks bubble once the enable sync flow has
  // started.
}
