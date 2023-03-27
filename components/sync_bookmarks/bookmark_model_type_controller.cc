// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_controller.h"

#include <utility>

#include "components/bookmarks/common/bookmark_features.h"

namespace sync_bookmarks {

BookmarkModelTypeController::BookmarkModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : ModelTypeController(syncer::BOOKMARKS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)) {}

BookmarkModelTypeController::~BookmarkModelTypeController() = default;

bool BookmarkModelTypeController::ShouldRunInTransportOnlyMode() const {
  return base::FeatureList::IsEnabled(
      bookmarks::kEnableBookmarksAccountStorage);
}

}  // namespace sync_bookmarks
