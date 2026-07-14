// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_SERVICE_FEATURE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_SERVICE_FEATURE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class BookmarkMergedSurfaceService;

namespace bookmarks_api {
class BookmarksService;
}

// BookmarksServiceFeature manages the lifecycle of BookmarksService.
// It observes BookmarkMergedSurfaceService to initialize the service once the
// model is loaded.
// If the underlying BookmarkMergedSurfaceService is destroyed (e.g. during
// profile shutdown), this feature enters a terminal shutdown state where it
// disconnects all active and pending connections and rejects new ones.
class BookmarksServiceFeature : public BookmarkMergedSurfaceServiceObserver {
 public:
  explicit BookmarksServiceFeature(
      BookmarkMergedSurfaceService* merged_service);
  ~BookmarksServiceFeature() override;

  // Accepts an incoming connection. Note that if the underlying bookmarks
  // model is not ready yet, the acceptance will be deferred.
  void Accept(
      mojo::PendingReceiver<bookmarks_api::mojom::BookmarksService> receiver);

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override;
  void BookmarkMergedSurfaceServiceBeingDeleted() override;
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override;
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override;
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override;
  void BookmarkAllUserNodesRemoved() override;

 private:
  // Initializes the service and attached any pending clients. Safe to call
  // multiple times, but the service will only be instantiated once.
  void InitializeService();

  // Shuts down the service, disconnecting all active and pending connections.
  // Once shut down, the feature is in a terminal state and cannot be
  // re-initialized.
  void ShutdownService();

  raw_ptr<BookmarkMergedSurfaceService> merged_service_;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      observation_{this};
  std::unique_ptr<bookmarks_api::BookmarksService> bookmarks_service_;
  std::vector<mojo::PendingReceiver<bookmarks_api::mojom::BookmarksService>>
      queued_receivers_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_SERVICE_FEATURE_H_
