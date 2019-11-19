// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_

#include <map>
#include <utility>

#include "base/callback_forward.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace base {
struct UserMetricsAction;
}

namespace bookmarks {

class BookmarkModel;
class BookmarkNode;
class BookmarkPermanentNode;

// This class abstracts operations that depends on the embedder's environment,
// e.g. Chrome.
class BookmarkClient {
 public:
  // Type representing a mapping from URLs to the number of times the URL has
  // been typed by the user in the Omnibox.
  using UrlTypedCountMap = std::unordered_map<const GURL*, int>;

  virtual ~BookmarkClient() {}

  // Called during initialization of BookmarkModel.
  virtual void Init(BookmarkModel* model);

  // Returns true if the embedder favors touch icons over favicons.
  virtual bool PreferTouchIcon();

  // Requests a favicon from the history cache for the web page at |page_url|.
  // |callback| is run when the favicon has been fetched. If |type| is:
  // - favicon_base::IconType::kFavicon, the returned gfx::Image is a
  //   multi-resolution image of gfx::kFaviconSize DIP width and height. The
  //   data from the history cache is resized if need be.
  // - not favicon_base::IconType::kFavicon, the returned gfx::Image is a
  //   single-resolution image with the largest bitmap in the history cache for
  //   |page_url| and |type|.
  virtual base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::IconType type,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker);

  // Returns true if the embedder supports typed count for URL.
  virtual bool SupportsTypedCountForUrls();

  // Retrieves the number of times each bookmark URL has been typed in
  // the Omnibox by the user. For each key (URL) in |url_typed_count_map|,
  // the corresponding value will be updated with the typed count of that URL.
  // |url_typed_count_map| must not be null.
  virtual void GetTypedCountForUrls(UrlTypedCountMap* url_typed_count_map);

  // Returns whether the embedder wants permanent node |node|
  // to always be visible or to only show them when not empty.
  virtual bool IsPermanentNodeVisible(const BookmarkPermanentNode* node) = 0;

  // Wrapper around RecordAction defined in base/metrics/user_metrics.h
  // that ensure that the action is posted from the correct thread.
  virtual void RecordAction(const base::UserMetricsAction& action) = 0;

  // Returns a task that will be used to load a managed root node. This task
  // will be invoked in the Profile's IO task runner.
  virtual LoadManagedNodeCallback GetLoadManagedNodeCallback() = 0;

  // Returns true if the |permanent_node| can have its title updated.
  virtual bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) = 0;

  // Returns true if |node| should sync.
  virtual bool CanSyncNode(const BookmarkNode* node) = 0;

  // Returns true if this node can be edited by the user.
  // TODO(joaodasilva): the model should check this more aggressively, and
  // should give the client a means to temporarily disable those checks.
  // http://crbug.com/49598
  virtual bool CanBeEditedByUser(const BookmarkNode* node) = 0;

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
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_CLIENT_H_
