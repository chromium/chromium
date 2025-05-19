// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_

#include "base/functional/callback_helpers.h"
#include "ui/base/interaction/element_identifier.h"

class Browser;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Lists the different versions of the BookmarkAccountStorageMoveDialog.
// These values are persisted to UMA. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(BookmarkAccountStorageMoveDialogType)
enum class BookmarkAccountStorageMoveDialogType {
  // Dialog triggered upon a user action where the destination is specified,
  // e.g. from a drag and drop in the bookmarks bar.
  kDownloadOrUpload = 0,
  // Dialog triggered by asking the user to upload the bookmark node (without
  // specifying the destination), e.g. from the bookmark manager.
  kUpload = 1,
  kMaxValue = kUpload,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:BookmarkAccountStorageMoveDialogType)

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
// `dialog_type` specifies which version of the dialog should be displayed.
// Note: In incognito mode, the dialog will be shown on top of a browser for the
// Original Profile instead of `browser`. It will create a new browser window if
// one doesn't exist already.
void ShowBookmarkAccountStorageMoveDialog(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    BookmarkAccountStorageMoveDialogType dialog_type,
    base::OnceClosure closed_callback = base::DoNothing());

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_
