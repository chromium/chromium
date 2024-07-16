// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_controller.h"

#include <utility>

namespace sync_bookmarks {

BookmarkModelTypeController::BookmarkModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode,
    std::unique_ptr<syncer::ModelTypeLocalDataBatchUploader> batch_uploader)
    : ModelTypeController(syncer::BOOKMARKS,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode),
                          std::move(batch_uploader)) {}

BookmarkModelTypeController::~BookmarkModelTypeController() = default;

}  // namespace sync_bookmarks
