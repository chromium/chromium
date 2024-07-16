// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/sync/service/local_data_description.h"

namespace sync_bookmarks {

void BookmarkLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  std::move(callback).Run({});
}

void BookmarkLocalDataBatchUploader::TriggerLocalDataMigration() {}

}  // namespace sync_bookmarks
