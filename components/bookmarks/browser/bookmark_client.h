// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace bookmarks {

class BookmarkModel;
class BookmarkPermanentNode;

// A callback that generates a std::unique_ptr<BookmarkPermanentNode>, given a
// max ID to use. The max ID argument will be updated after if a new node has
// been created and assigned an ID.
using LoadManagedNodeCallback =
    base::OnceCallback<std::unique_ptr<BookmarkPermanentNode>(int64_t*)>;

// This class abstracts operations that depends on the embedder's environment,
// e.g. Chrome.
class BookmarkClient {
 public:
  // Type representing a mapping from URLs to the number of times the URL has
  // been typed by the user in the Omnibox.
  using UrlTypedCountMap = std::unordered_map<const GURL*, int>;

  virtual ~BookmarkClient() = default;

  // Returns whether the embedder wants permanent node of type |type|
  // to always be visible or to only show them when not empty.
  bool IsPermanentNodeVisibleWhenEmpty(BookmarkNode::Type type) const;

  // Called during initialization of BookmarkModel.
  virtual void Init(BookmarkModel* model);

  // Gets a bookmark folder that the provided URL can be saved to. If nullptr is
  // returned, the bookmark is saved to the default location (usually this is
  // the last modified folder). This affords features the option to override the
  // default folder if relevant for the URL.
  virtual const BookmarkNode* GetSuggestedSaveLocation(const GURL& url);

  // Requests a favicon from the history cache for the web page at |page_url|
  // for icon type favicon_base::IconType::kFavicon. |callback| is run when the
  // favicon has been fetched, which returns gfx::Image is a multi-resolution
  // image of gfx::kFaviconSize DIP width and height. The data from the history
  // cache is resized if need be.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker);

  // Returns true if the embedder supports typed count for URL.
  virtual bool SupportsTypedCountForUrls();

  // Retrieves the number of times each bookmark URL has been typed in
  // the Omnibox by the user. For each key (URL) in |url_typed_count_map|,
  // the corresponding value will be updated with the typed count of that URL.
  // |url_typed_count_map| must not be null.
  virtual void GetTypedCountForUrls(UrlTypedCountMap* url_typed_count_map);

  // Returns a task that will be used to load a managed root node. This task
  // will be invoked in the Profile's IO task runner.
  virtual LoadManagedNodeCallback GetLoadManagedNodeCallback() = 0;

  // Returns the current storage state to be added as suffix to metrics.
  virtual metrics::StorageStateForUma GetStorageStateForUma() = 0;

  // Returns true if the |permanent_node| can have its title updated.
  virtual bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) = 0;

  // Returns true if |node| is considered a managed node.
  virtual bool IsNodeManaged(const BookmarkNode* node) = 0;

  // Encodes the bookmark sync data into a string blob. It's used by the
  // bookmark model to persist the sync metadata together with the bookmark
  // model.
  virtual std::string EncodeBookmarkSyncMetadata() = 0;

  // Decodes a string represeting the sync metadata stored in |metadata_str|.
  // The model calls this method after it has loaded the model data.
  // |schedule_save_closure| is a repeating call back to trigger a model and
  // metadata persistence process.
  virtual void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) = 0;

  // Similar to BookmarkModelObserver::BookmarkNodeRemoved(), but transfers
  // ownership of BookmarkNode, which allows undoing the operation.
  virtual void OnBookmarkNodeRemovedUndoable(
      BookmarkModel* model,
      const BookmarkNode* parent,
      size_t index,
      std::unique_ptr<BookmarkNode> node) = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_
