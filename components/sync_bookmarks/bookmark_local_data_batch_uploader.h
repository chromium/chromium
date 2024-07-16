// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_

#include "components/sync/service/model_type_local_data_batch_uploader.h"

namespace sync_bookmarks {

class BookmarkLocalDataBatchUploader
    : public syncer::ModelTypeLocalDataBatchUploader {
 public:
  BookmarkLocalDataBatchUploader() = default;

  BookmarkLocalDataBatchUploader(const BookmarkLocalDataBatchUploader&) =
      delete;
  BookmarkLocalDataBatchUploader& operator=(
      const BookmarkLocalDataBatchUploader&) = delete;

  ~BookmarkLocalDataBatchUploader() override = default;

  // syncer::ModelTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_DATA_BATCH_UPLOADER_H_
