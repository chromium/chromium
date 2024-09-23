// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_sync_service.h"

#include <utility>

#include "base/feature_list.h"
#include "components/undo/bookmark_undo_service.h"

namespace sync_bookmarks {

BookmarkSyncService::BookmarkSyncService(
    BookmarkUndoService* bookmark_undo_service,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior)
    : bookmark_data_type_processor_(bookmark_undo_service,
                                    wipe_model_upon_sync_disabled_behavior) {}

BookmarkSyncService::~BookmarkSyncService() = default;

std::string BookmarkSyncService::EncodeBookmarkSyncMetadata() {
  return bookmark_data_type_processor_.EncodeSyncMetadata();
}

void BookmarkSyncService::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    std::unique_ptr<sync_bookmarks::BookmarkModelView> model) {
  bookmark_model_view_ = std::move(model);
  bookmark_data_type_processor_.ModelReadyToSync(
      metadata_str, schedule_save_closure, bookmark_model_view_.get());
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
BookmarkSyncService::GetBookmarkSyncControllerDelegate(
    favicon::FaviconService* favicon_service) {
  DCHECK(favicon_service);
  bookmark_data_type_processor_.SetFaviconService(favicon_service);
  return bookmark_data_type_processor_.GetWeakPtr();
}

bool BookmarkSyncService::IsTrackingMetadata() const {
  return bookmark_data_type_processor_.IsTrackingMetadata() ||
         is_tracking_metadata_for_testing_;
}

sync_bookmarks::BookmarkModelView* BookmarkSyncService::bookmark_model_view() {
  return bookmark_model_view_.get();
}

void BookmarkSyncService::SetIsTrackingMetadataForTesting() {
  is_tracking_metadata_for_testing_ = true;
}

void BookmarkSyncService::SetBookmarksLimitForTesting(size_t limit) {
  bookmark_data_type_processor_
      .SetMaxBookmarksTillSyncEnabledForTest(  // IN-TEST
          limit);
}

}  // namespace sync_bookmarks
