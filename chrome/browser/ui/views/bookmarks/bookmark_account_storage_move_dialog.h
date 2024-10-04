// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"

class Browser;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// IDs for the button dialogs, to allow pressing them in tests.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogOkButton);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogCancelButton);

// Shows a modal dialog in `browser`'s window that offers to move `node` to a
// `target_folder` from a different bookmark storage. That is, either `node` is
// local-only and `target_folder` is in the signed-in account, or the other way
// around. `closed_callback` will be invoked once the dialog is closed, either
// because the user accepted the move or canceled it (current callers don't need
// to distinguish the 2 cases). If the user accepts, `node` will become the
// `index`-th child of `target_folder`.
// Must only be called if there is a signed-in account, and if `node` and
// `target_folder` have different storages, otherwise the call will crash.
void ShowBookmarkAccountStorageMoveDialog(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    base::OnceClosure closed_callback);

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_
