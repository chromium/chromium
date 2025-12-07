// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/bookmark_bar_page_handler.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace {

bookmark_bar::mojom::BookmarkType ConvertType(
    bookmarks::BookmarkNode::Type type) {
  switch (type) {
    case bookmarks::BookmarkNode::URL:
      return bookmark_bar::mojom::BookmarkType::URL;
    case bookmarks::BookmarkNode::FOLDER:
      return bookmark_bar::mojom::BookmarkType::FOLDER;
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
      return bookmark_bar::mojom::BookmarkType::BOOKMARK_BAR;
    case bookmarks::BookmarkNode::OTHER_NODE:
      return bookmark_bar::mojom::BookmarkType::OTHER_NODE;
    case bookmarks::BookmarkNode::MOBILE:
      return bookmark_bar::mojom::BookmarkType::MOBILE;
  }
  NOTREACHED();
}

}  // namespace

WebUIBrowserBookmarkBarPageHandler::WebUIBrowserBookmarkBarPageHandler(
    mojo::PendingReceiver<bookmark_bar::mojom::PageHandler> receiver,
    mojo::PendingRemote<bookmark_bar::mojom::Page> page,
    content::WebUI* web_ui,
    Browser* browser)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui),
      browser_(browser) {
  bookmark_model_ =
      BookmarkModelFactory::GetForBrowserContext(browser_->profile());

  if (bookmark_model_) {
    bookmark_model_->AddObserver(this);
    if (bookmark_model_->loaded()) {
      BookmarkModelLoaded(false);
    }
    // else case: we'll receive notification back from the BookmarkModel when
    // done loading, then we'll populate the bar.
  }
}

WebUIBrowserBookmarkBarPageHandler::~WebUIBrowserBookmarkBarPageHandler() {
  if (bookmark_model_) {
    bookmark_model_->RemoveObserver(this);
  }
}

void WebUIBrowserBookmarkBarPageHandler::SetBookmarkBarState(
    BookmarkBar::State state,
    BookmarkBar::AnimateChangeType animate_type) {
  // TODO(webium): Do we care about disabling animation for parity in Webium?
  if (bookmark_bar_state_ == state) {
    return;
  }

  if (state == BookmarkBar::SHOW) {
    page_->Show();
  } else {
    page_->Hide();
  }

  bookmark_bar_state_ = state;
}

bookmark_bar::mojom::BookmarkDataPtr
WebUIBrowserBookmarkBarPageHandler::GetBookmarkData(
    const bookmarks::BookmarkNode* node) {
  auto bookmark_data = bookmark_bar::mojom::BookmarkData::New();
  bookmark_data->title = base::UTF16ToUTF8(node->GetTitle());
  bookmark_data->id = node->id();
  bookmark_data->type = ConvertType(node->type());
  if (!node->is_favicon_loaded() && !node->is_favicon_loading()) {
    // Trigger a request to fetch fav icon.
    bookmark_model_->GetFavicon(node);
  } else if (node->icon_url()) {
    bookmark_data->page_url_for_favicon = node->url();
  }

  return bookmark_data;
}

void WebUIBrowserBookmarkBarPageHandler::GetBookmarkBar(
    GetBookmarkBarCallback callback) {
  std::vector<bookmark_bar::mojom::BookmarkDataPtr> bookmarks;

  if (bookmark_model_->loaded() &&
      !bookmark_model_->bookmark_bar_node()->children().empty()) {
    // This is calculated by the display size
    // Will need to be fetched from the UI? Or perhaps just send all bookmarks
    // to the WebUI? For now just pick a random number.
    const int max_bookmarks = 20;
    const int bookmark_model_count =
        bookmark_model_->loaded()
            ? bookmark_model_->bookmark_bar_node()->children().size()
            : 0;
    int bookmark_count = std::min(max_bookmarks, bookmark_model_count);
    for (int i = 0; i < bookmark_count; i++) {
      bookmarks.push_back(GetBookmarkData(
          bookmark_model_->bookmark_bar_node()->children()[i].get()));
    }
  }

  std::move(callback).Run(std::move(bookmarks));
}

void WebUIBrowserBookmarkBarPageHandler::OpenInNewTab(int64_t node_id) {
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
  bookmarks::OpenAllIfAllowed(
      browser_, {node}, WindowOpenDisposition::CURRENT_TAB,
      bookmarks::OpenAllBookmarksContext::kNone,
      page_load_metrics::NavigationHandleUserData::InitiatorLocation::
          kBookmarkBar,
      {{BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()}});
}

// bookmarks::BookmarkModelObserver:
void WebUIBrowserBookmarkBarPageHandler::BookmarkModelLoaded(
    bool ids_reassigned) {
  page_->BookmarkLoaded();
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkModelBeingDeleted() {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  // TODO(webium): Implement.
}

void WebUIBrowserBookmarkBarPageHandler::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  if (node->is_favicon_loaded()) {
    page_->FavIconChanged(GetBookmarkData(node));
  }
}
