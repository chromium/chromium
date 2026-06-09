// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"
#include "components/sync_bookmarks/constants.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

class PrefService;

namespace sync_bookmarks {

class BookmarkModelViewUsingLocalOrSyncableNodes;

class BookmarkLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  // `bookmark_model` must either be null or non-null and outlive this object.
  // `pref_service` mut not be null and must outlive this object.
  BookmarkLocalDataBatchUploader(bookmarks::BookmarkModel* bookmark_model,
                                 PrefService* pref_service);

  BookmarkLocalDataBatchUploader(const BookmarkLocalDataBatchUploader&) =
      delete;
  BookmarkLocalDataBatchUploader& operator=(
      const BookmarkLocalDataBatchUploader&) = delete;

  ~BookmarkLocalDataBatchUploader() override;

  // syncer::DataTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;
  void TriggerLocalDataMigrationForItems(
      std::vector<syncer::LocalDataItemModel::DataId> items) override;

  void SetMaxBookmarksLimitForTesting(size_t limit);

 private:
  // TODO(crbug.com/516993407): CanUploadResult and DetermineAbilityToUpload()
  // exist solely to support correct metric recording
  // (Sync.BatchUpload.BookmarksDisabledDueToLimitExceeded) in
  // GetLocalDataDescription(). If this metric is removed, this should be
  // simplified back to a simple boolean check.
  enum class CanUploadResult {
    kAllowed,
    kNotAllowed,
    kLimitExceeded,
  };

  // Determines the ability to upload local bookmarks.
  // Note: The order of evaluation in this method is important. Specifically,
  // the presence of local data to upload must be checked BEFORE checking if the
  // limit is exceeded. This ensures that the limit-exceeded metric is only
  // recorded when there is actually data to upload, avoiding skewing the
  // metric.
  CanUploadResult DetermineAbilityToUpload() const;
  bool CanUpload() const;

  // Returns the URLs of all the bookmarked items in the subtree (including
  // subtree_root).
  std::vector<GURL> GetBookmarkedUrlsInSubtree(
      const BookmarkModelViewUsingLocalOrSyncableNodes&
          local_or_syncable_bookmark_model_view,
      const bookmarks::BookmarkNode* subtree_root) const;

  // Returns the `LocalDataItemModel` corresponding to the given `node`.
  //
  // For folders:
  // - title: <folder title>
  // - subtitle: "<N> bookmarks", where N is the total number of children
  //             (folders or bookmarks)
  //
  // For bookmarks:
  // - title: <bookmark title>
  // - subtitle: empty
  syncer::LocalDataItemModel DataItemModelFromNode(
      const bookmarks::BookmarkNode* node,
      int bookmarked_urls_count) const;

  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  const raw_ptr<PrefService> pref_service_;
  size_t max_bookmarks_limit_ = kSyncBookmarksLimit;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_
