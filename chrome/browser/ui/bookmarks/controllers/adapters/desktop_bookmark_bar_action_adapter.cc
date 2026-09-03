// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_action_adapter.h"

#include "base/containers/to_vector.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/profile_metrics/browser_profile_type.h"
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

void DesktopBookmarkBarActionAdapter::OpenBookmark(
    int64_t node_id,
    WindowOpenDisposition disposition) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile());
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, node_id);
  if (!node) {
    return;
  }

  RecordAppLaunchForBookmarkBar(browser_->GetProfile(), node->url());
  bookmarks::OpenAllIfAllowed(
      browser_, {node}, disposition, bookmarks::OpenAllBookmarksContext::kNone,
      GetInitiatorLocation(ChromeInitiatorLocation::kBookmarkBar),
      {{BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()}});
  RecordBookmarkLaunch(
      BookmarkLaunchLocation::kAttachedBar,
      profile_metrics::GetBrowserProfileType(browser_->GetProfile()));
  chrome::UpdateBookmarkBarVisibilityPrefOnUserAction(browser_->GetProfile());
}

void DesktopBookmarkBarActionAdapter::NotifyFolderOpened() {
  chrome::UpdateBookmarkBarVisibilityPrefOnUserAction(browser_->GetProfile());
  RecordBookmarkFolderOpen(BookmarkLaunchLocation::kAttachedBar);
}

void DesktopBookmarkBarActionAdapter::OpenFolderNodes(
    const bookmarks::BookmarkNodeId& folder_id,
    WindowOpenDisposition disposition) {
  BookmarkParentFolder folder = chrome::ToFolder(
      folder_id,
      BookmarkModelFactory::GetForBrowserContext(browser_->GetProfile()));
  chrome::UpdateBookmarkBarVisibilityPrefOnUserAction(browser_->GetProfile());

  RecordBookmarkFolderLaunch(BookmarkLaunchLocation::kAttachedBar);
  BookmarkMergedSurfaceService* service =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(
          browser_->GetProfile());
  auto nodes = base::ToVector(
      service->GetUnderlyingNodes(folder),
      [](const bookmarks::BookmarkNode* node) {
        return raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>(node);
      });

  bookmarks::OpenAllIfAllowed(
      browser_, nodes, disposition, bookmarks::OpenAllBookmarksContext::kNone,
      GetInitiatorLocation(ChromeInitiatorLocation::kBookmarkBar),
      {{BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()}});
}
