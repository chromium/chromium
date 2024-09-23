// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_

#include <memory>

#include "components/sync/service/data_type_local_data_batch_uploader.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace sync_bookmarks {

class BookmarkModelView;

class BookmarkLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  // `bookmark_model` must either be null or non-null and outlive this object.
  explicit BookmarkLocalDataBatchUploader(
      bookmarks::BookmarkModel* bookmark_model);

  BookmarkLocalDataBatchUploader(const BookmarkLocalDataBatchUploader&) =
      delete;
  BookmarkLocalDataBatchUploader& operator=(
      const BookmarkLocalDataBatchUploader&) = delete;

  ~BookmarkLocalDataBatchUploader() override;

  // syncer::DataTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;

 private:
  bool CanUpload() const;

  const std::unique_ptr<BookmarkModelView>
      local_or_syncable_bookmark_model_view_;
  const std::unique_ptr<BookmarkModelView> account_bookmark_model_view_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_
