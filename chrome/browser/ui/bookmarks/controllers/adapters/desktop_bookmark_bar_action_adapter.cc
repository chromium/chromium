// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_action_adapter.h"

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "chrome/browser/page_load_metrics/chrome_initiator_location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/base_window.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

DesktopBookmarkBarActionAdapter::DesktopBookmarkBarActionAdapter(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
}

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
    const bookmarks::BookmarkNode* node,
    WindowOpenDisposition disposition) {
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
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    WindowOpenDisposition disposition) {
  if (nodes.empty()) {
    return;
  }

  chrome::UpdateBookmarkBarVisibilityPrefOnUserAction(browser_->GetProfile());
  RecordBookmarkFolderLaunch(BookmarkLaunchLocation::kAttachedBar);

  auto raw_nodes =
      base::ToVector(nodes, [](const bookmarks::BookmarkNode* node) {
        return raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>(node);
      });

  bookmarks::OpenAllIfAllowed(
      browser_, raw_nodes, disposition,
      bookmarks::OpenAllBookmarksContext::kNone,
      GetInitiatorLocation(ChromeInitiatorLocation::kBookmarkBar),
      {{BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()}});
}

void DesktopBookmarkBarActionAdapter::ShowContextMenu(
    const std::vector<const bookmarks::BookmarkNode*>& selection,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    bool can_paste,
    base::OnceClosure on_close) {
  context_menu_observation_.Reset();
  if (on_context_menu_closed_callback_) {
    std::move(on_context_menu_closed_callback_).Run();
  }
  on_context_menu_closed_callback_ = std::move(on_close);

  auto raw_selection =
      base::ToVector(selection, [](const bookmarks::BookmarkNode* node) {
        return raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>(node);
      });

  context_menu_ = std::make_unique<BookmarkContextMenu>(
      views::Widget::GetWidgetForNativeWindow(
          browser_->GetWindow()->GetNativeWindow()),
      browser_, browser_->GetProfile(), BookmarkLaunchLocation::kAttachedBar,
      raw_selection, /*close_on_remove=*/true, can_paste);
  context_menu_observation_.Observe(context_menu_.get());
  context_menu_->RunMenuAt(point, source_type);
}

void DesktopBookmarkBarActionAdapter::OnContextMenuClosed() {
  context_menu_observation_.Reset();
  if (on_context_menu_closed_callback_) {
    std::move(on_context_menu_closed_callback_).Run();
  }
}
