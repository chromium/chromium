// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_action_adapter.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/android/bookmark_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

using ui::mojom::WindowOpenDisposition;

std::vector<GURL> CollectUrlsToOpen(
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    WindowOpenDisposition disposition) {
  std::vector<GURL> urls;
  const auto maybe_add_url = [&](const GURL& url) {
    if (disposition != WindowOpenDisposition::kCurrentTab &&
        url.SchemeIs(url::kJavaScriptScheme)) {
      return;
    }
    urls.push_back(url);
  };

  for (const bookmarks::BookmarkNode* node : nodes) {
    if (!node) {
      continue;
    }
    if (node->is_url()) {
      maybe_add_url(node->url());
      continue;
    }
    for (const auto& child : node->children()) {
      if (child->is_url()) {
        maybe_add_url(child->url());
      }
    }
  }
  return urls;
}

// `in_current_tab` picks the current tab or the multiple-opened metrics.
void RecordBookmarkOpened(const bookmarks::BookmarkNode& node,
                          bool in_current_tab) {
  const std::string prefix =
      in_current_tab ? "Bookmarks." : "Bookmarks.MultipleOpened.";
  base::RecordAction(
      in_current_tab
          ? base::UserMetricsAction("MobileBookmarkManagerEntryOpened")
          : base::UserMetricsAction(
                "MobileBookmarkManagerMultipleEntriesOpened"));
  base::UmaHistogramExactLinear(prefix + "OpenBookmarkType",
                                bookmarks::BOOKMARK_TYPE_NORMAL,
                                bookmarks::BOOKMARK_TYPE_LAST + 1);
  // The histogram is shared with Java, which clamps its 30 day max to int ms.
  base::UmaHistogramCustomTimes(
      prefix + "OpenBookmarkTimeInterval2.Normal",
      base::Time::Now() - node.date_added(), base::Milliseconds(1),
      base::Milliseconds(std::numeric_limits<int32_t>::max() - 1), 50);
  base::UmaHistogramBoolean("Bookmarks.MobileBookmarkManager.OpenedURL", true);
}

// Returns the active tab's contents if a bookmarklet can run on it, or null.
content::WebContents* GetBookmarkletContents(BrowserWindowInterface* browser) {
  TabListInterface* tab_list = TabListInterface::From(browser);
  TabAndroid* tab = TabAndroid::FromTabInterface(
      tab_list ? tab_list->GetActiveTab() : nullptr);
  if (!tab || !tab->web_contents() || tab->IsNativePage()) {
    return nullptr;
  }
  return tab->web_contents();
}

void SetBookmarkId(int64_t bookmark_id, content::NavigationHandle& handle) {
  if (auto* ui_data =
          static_cast<ChromeNavigationUIData*>(handle.GetNavigationUIData())) {
    ui_data->set_bookmark_id(bookmark_id);
  }
}

// Android can't save to disk or open split view, among others.
bool IsSupportedDisposition(WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::kCurrentTab:
    case WindowOpenDisposition::kNewForegroundTab:
    case WindowOpenDisposition::kNewBackgroundTab:
    case WindowOpenDisposition::kNewWindow:
      return true;
    default:
      return false;
  }
}

}  // namespace

AndroidBookmarkBarActionAdapter::AndroidBookmarkBarActionAdapter(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
}

AndroidBookmarkBarActionAdapter::~AndroidBookmarkBarActionAdapter() = default;

void AndroidBookmarkBarActionAdapter::OpenAppsPage(
    ui::mojom::WindowOpenDisposition disposition) {
  // Android has no chrome://apps page or apps shortcut on the bookmark bar.
}

void AndroidBookmarkBarActionAdapter::OpenBookmark(
    const bookmarks::BookmarkNode* node,
    ui::mojom::WindowOpenDisposition disposition) {
  if (!IsSupportedDisposition(disposition)) {
    return;
  }
  if (node->url().SchemeIs(url::kJavaScriptScheme)) {
    // Only passed with kCurrentTab. It runs without a navigation, so it can't
    // go through `OpenURL()`.
    content::WebContents* contents = GetBookmarkletContents(browser_);
    if (!contents) {
      return;
    }
    RecordBookmarkOpened(*node, /*in_current_tab=*/true);
    content::NavigationController::LoadURLParams params(node->url());
    params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    contents->GetController().LoadURLWithParams(params);
    return;
  }
  RecordBookmarkOpened(*node,
                       disposition == WindowOpenDisposition::kCurrentTab);
  bookmarks::OpenBookmarkUrl(browser_, node->url(), disposition,
                             base::BindOnce(&SetBookmarkId, node->id()));
}

void AndroidBookmarkBarActionAdapter::RecordFolderOpened() {
  // Desktop calls RecordBookmarkFolderOpen() here; Android records
  // Bookmarks.BookmarkBar.Click on the Java side instead.
}

void AndroidBookmarkBarActionAdapter::OpenFolderNodes(
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    ui::mojom::WindowOpenDisposition disposition) {
  bookmarks::OpenBookmarkUrls(browser_, CollectUrlsToOpen(nodes, disposition),
                              disposition);
}

void AndroidBookmarkBarActionAdapter::CanPasteFromClipboard(
    const std::vector<const bookmarks::BookmarkNode*>& selection,
    base::OnceCallback<void(bool)> callback) {
  // Android's bookmark bar shows its context menu in Java without going through
  // BookmarkBarUIController.
  std::move(callback).Run(false);
}

void AndroidBookmarkBarActionAdapter::ShowContextMenu(
    const std::vector<const bookmarks::BookmarkNode*>& selection,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type,
    bool can_paste,
    base::OnceClosure on_close) {
  // Android's bookmark bar shows its context menu in Java without going through
  // BookmarkBarUIController.
  std::move(on_close).Run();
}
