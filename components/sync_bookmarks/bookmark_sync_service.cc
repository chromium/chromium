// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_sync_service.h"

#include "base/feature_list.h"
#include "components/undo/bookmark_undo_service.h"

namespace sync_bookmarks {

BookmarkSyncService::BookmarkSyncService(
    BookmarkUndoService* bookmark_undo_service,
    bool wipe_model_on_stopping_sync_with_clear_data)
    : bookmark_model_type_processor_(
          bookmark_undo_service,
          wipe_model_on_stopping_sync_with_clear_data) {}

BookmarkSyncService::~BookmarkSyncService() = default;

std::string BookmarkSyncService::EncodeBookmarkSyncMetadata() {
  return bookmark_model_type_processor_.EncodeSyncMetadata();
}

void BookmarkSyncService::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    bookmarks::BookmarkModel* model) {
  bookmark_model_type_processor_.ModelReadyToSync(metadata_str,
                                                  schedule_save_closure, model);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
BookmarkSyncService::GetBookmarkSyncControllerDelegate(
    favicon::FaviconService* favicon_service) {
  DCHECK(favicon_service);
  bookmark_model_type_processor_.SetFaviconService(favicon_service);
  return bookmark_model_type_processor_.GetWeakPtr();
}

bool BookmarkSyncService::IsTrackingMetadata() const {
  return bookmark_model_type_processor_.IsTrackingMetadata();
}

void BookmarkSyncService::SetBookmarksLimitForTesting(size_t limit) {
  bookmark_model_type_processor_
      .SetMaxBookmarksTillSyncEnabledForTest(  // IN-TEST
          limit);
}

}  // namespace sync_bookmarks
