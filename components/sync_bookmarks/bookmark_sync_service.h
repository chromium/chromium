// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "components/sync_bookmarks/bookmark_data_type_processor.h"
#include "components/sync_bookmarks/bookmark_model_view.h"

class BookmarkUndoService;

namespace syncer {
class DataTypeControllerDelegate;
}

namespace bookmarks {
class BookmarkModel;
}

namespace favicon {
class FaviconService;
}

namespace sync_bookmarks {
class BookmarkDataTypeProcessor;

// This service owns the BookmarkDataTypeProcessor.
class BookmarkSyncService : public KeyedService {
 public:
  // `bookmark_undo_service` must not be null and must outlive this object.
  BookmarkSyncService(BookmarkUndoService* bookmark_undo_service,
                      syncer::WipeModelUponSyncDisabledBehavior
                          wipe_model_upon_sync_disabled_behavior);

  BookmarkSyncService(const BookmarkSyncService&) = delete;
  BookmarkSyncService& operator=(const BookmarkSyncService&) = delete;

  // KeyedService implemenation.
  ~BookmarkSyncService() override;

  // Analgous to Encode/Decode methods in BookmarkClient.
  std::string EncodeBookmarkSyncMetadata();
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure,
      std::unique_ptr<sync_bookmarks::BookmarkModelView> model);

  // Returns the DataTypeControllerDelegate for syncer::BOOKMARKS.
  // `favicon_service` is the favicon service used when processing updates in
  // the underlying processor. It could have been a separate a setter in
  // BookmarkSyncService instead of passing it as a parameter to
  // GetBookmarkSyncControllerDelegate(). However, this would incur the risk of
  // overlooking setting it. Therefore, it has been added as a parameter to the
  // GetBookmarkSyncControllerDelegate() in order to gauarantee it will be set
  // before the processor starts receiving updates.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetBookmarkSyncControllerDelegate(favicon::FaviconService* favicon_service);

  // Returns true if sync metadata is being tracked. This means sync is enabled
  // and the initial download of data is completed, which implies that the
  // relevant BookmarkModel already reflects remote data. Note however that this
  // doesn't mean bookmarks are actively sync-ing at the moment, for example
  // sync could be paused due to an auth error.
  bool IsTrackingMetadata() const;

  // Returns the BookmarkModelView representing the subset of bookmarks that
  // this service is dealing with (potentially sync-ing, but not necessarily).
  // It returns null until bookmarks are loaded, i.e. until
  // DecodeBookmarkSyncMetadata() is invoked. It must not be invoked after
  // Shutdown(), i.e. during profile destruction.
  sync_bookmarks::BookmarkModelView* bookmark_model_view();

  // For integration tests.
  void SetIsTrackingMetadataForTesting();
  void SetBookmarksLimitForTesting(size_t limit);

 private:
  std::unique_ptr<BookmarkModelView> bookmark_model_view_;
  // BookmarkDataTypeProcessor handles communications between sync engine and
  // BookmarkModel/HistoryService.
  BookmarkDataTypeProcessor bookmark_data_type_processor_;
  bool is_tracking_metadata_for_testing_ = false;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_SYNC_SERVICE_H_
