// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_action_adapter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "url/gurl.h"

DesktopBookmarkBarActionAdapter::DesktopBookmarkBarActionAdapter(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

DesktopBookmarkBarActionAdapter::~DesktopBookmarkBarActionAdapter() = default;

void DesktopBookmarkBarActionAdapter::OpenAppsPage(
    WindowOpenDisposition disposition) {
  content::OpenURLParams params(GURL(chrome::kChromeUIAppsURL),
                                content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser_->OpenURL(params, /*navigation_handle_callback=*/{});
  RecordBookmarkAppsPageOpen(BookmarkLaunchLocation::kAttachedBar);
  chrome::UpdateBookmarkBarVisibilityPrefOnUserAction(browser_->GetProfile());
}
