// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_action_adapter.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

using ui::mojom::WindowOpenDisposition;

void OpenUrlInBrowser(
    BrowserWindowInterface* browser,
    const GURL& url,
    WindowOpenDisposition disposition,
    base::OnceCallback<void(content::NavigationHandle&)> callback = {}) {
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url, disposition, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  browser->OpenURL(params, std::move(callback));
}

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
  if (!node || !node->is_url()) {
    return;
  }
  if (disposition != WindowOpenDisposition::kCurrentTab &&
      node->url().SchemeIs(url::kJavaScriptScheme)) {
    return;
  }
  OpenUrlInBrowser(browser_, node->url(), disposition);
}

void AndroidBookmarkBarActionAdapter::RecordFolderOpened() {
  // Desktop calls RecordBookmarkFolderOpen() here; Android records
  // Bookmarks.BookmarkBar.Click on the Java side instead.
}

void AndroidBookmarkBarActionAdapter::OpenFolderNodes(
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    ui::mojom::WindowOpenDisposition disposition) {
  std::vector<GURL> urls = CollectUrlsToOpen(nodes, disposition);
  if (urls.empty()) {
    return;
  }

  const bool opens_in_new_window =
      disposition == WindowOpenDisposition::kNewWindow ||
      disposition == WindowOpenDisposition::kOffTheRecord;
  if (opens_in_new_window && urls.size() > 1) {
    GURL first_url = urls.front();
    std::vector<GURL> remaining_urls(urls.begin() + 1, urls.end());
    OpenUrlInBrowser(
        browser_, first_url, disposition,
        base::BindOnce(
            [](std::vector<GURL> urls_to_open,
               content::NavigationHandle& handle) {
              content::WebContents* contents = handle.GetWebContents();
              if (!contents) {
                return;
              }
              tabs::TabInterface* tab =
                  tabs::TabInterface::GetFromContents(contents);
              if (!tab || !tab->GetBrowserWindowInterface()) {
                return;
              }
              BrowserWindowInterface* new_browser =
                  tab->GetBrowserWindowInterface();
              for (const GURL& url : urls_to_open) {
                OpenUrlInBrowser(new_browser, url,
                                 WindowOpenDisposition::kNewBackgroundTab);
              }
            },
            std::move(remaining_urls)));
    return;
  }

  for (const GURL& url : urls) {
    OpenUrlInBrowser(browser_, url, disposition);
  }
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
