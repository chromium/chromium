// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_BOOKMARK_BAR_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_BOOKMARK_BAR_PAGE_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/webui_browser/bookmark_bar.mojom.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class Browser;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

class WebUIBrowserBookmarkBarPageHandler
    : public bookmark_bar::mojom::PageHandler,
      public bookmarks::BookmarkModelObserver {
 public:
  WebUIBrowserBookmarkBarPageHandler(
      mojo::PendingReceiver<bookmark_bar::mojom::PageHandler> receiver,
      mojo::PendingRemote<bookmark_bar::mojom::Page> page,
      content::WebUI* web_ui,
      Browser* browser);
  WebUIBrowserBookmarkBarPageHandler(
      const WebUIBrowserBookmarkBarPageHandler&) = delete;
  WebUIBrowserBookmarkBarPageHandler& operator=(
      const WebUIBrowserBookmarkBarPageHandler&) = delete;
  ~WebUIBrowserBookmarkBarPageHandler() override;

  void SetBookmarkBarState(BookmarkBar::State state,
                           BookmarkBar::AnimateChangeType animate_type);

  // bookmark_bar::mojom::PageHandler
  void GetBookmarkBar(GetBookmarkBarCallback callback) override;
  void OpenInNewTab(int64_t node_ids) override;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;

 private:
  bookmark_bar::mojom::BookmarkDataPtr GetBookmarkData(
      const bookmarks::BookmarkNode* node);

  mojo::Receiver<bookmark_bar::mojom::PageHandler> receiver_;
  mojo::Remote<bookmark_bar::mojom::Page> page_;
  const raw_ptr<content::WebUI> web_ui_;
  const raw_ptr<Browser> browser_;

  BookmarkBar::State bookmark_bar_state_ = BookmarkBar::SHOW;

  // BookmarkModel that owns the entries and folders that are shown in this
  // view. This is owned by the Profile.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;

  base::WeakPtrFactory<WebUIBrowserBookmarkBarPageHandler> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_BOOKMARK_BAR_PAGE_HANDLER_H_
