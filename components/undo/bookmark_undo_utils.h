// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNDO_BOOKMARK_UNDO_UTILS_H_
#define COMPONENTS_UNDO_BOOKMARK_UNDO_UTILS_H_

#include "base/memory/raw_ptr.h"

class BookmarkUndoService;
class UndoManager;

// ScopedSuspendBookmarkUndo --------------------------------------------------

// Scopes the suspension of the undo tracking for non-user initiated changes
// such as those occuring during account synchronization.
class ScopedSuspendBookmarkUndo {
 public:
  explicit ScopedSuspendBookmarkUndo(
      BookmarkUndoService* bookmark_undo_service);

  ScopedSuspendBookmarkUndo(const ScopedSuspendBookmarkUndo&) = delete;
  ScopedSuspendBookmarkUndo& operator=(const ScopedSuspendBookmarkUndo&) =
      delete;

  ~ScopedSuspendBookmarkUndo();

 private:
  raw_ptr<UndoManager> undo_manager_;
};

#endif  // COMPONENTS_UNDO_BOOKMARK_UNDO_UTILS_H_
