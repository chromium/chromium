// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/bookmark_undo_utils.h"

#include "components/undo/bookmark_undo_service.h"
#include "components/undo/undo_manager.h"

// ScopedSuspendBookmarkUndo --------------------------------------------------

ScopedSuspendBookmarkUndo::ScopedSuspendBookmarkUndo(
    BookmarkUndoService* bookmark_undo_service)
    : undo_manager_(bookmark_undo_service
                        ? bookmark_undo_service->undo_manager()
                        : nullptr) {
  if (undo_manager_)
    undo_manager_->SuspendUndoTracking();
}

ScopedSuspendBookmarkUndo::~ScopedSuspendBookmarkUndo() {
  if (undo_manager_)
    undo_manager_->ResumeUndoTracking();
}
