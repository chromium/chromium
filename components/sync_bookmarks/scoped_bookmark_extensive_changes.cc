// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/scoped_bookmark_extensive_changes.h"

#include "components/bookmarks/browser/bookmark_model.h"

namespace sync_bookmarks {

ScopedBookmarkExtensiveChanges::ScopedBookmarkExtensiveChanges(
    bookmarks::BookmarkModel& model)
    : model_(model) {
  model_->BeginExtensiveChanges();
}

ScopedBookmarkExtensiveChanges::~ScopedBookmarkExtensiveChanges() {
  model_->EndExtensiveChanges();
}

}  // namespace sync_bookmarks
