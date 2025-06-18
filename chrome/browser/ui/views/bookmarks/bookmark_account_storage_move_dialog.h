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

// IDs for the button dialogs, to allow pressing them in tests.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogOkButton);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kBookmarkAccountStorageMoveDialogCancelButton);

// Shows a modal dialog in `browser`'s window that offers to move `node` to a
// `target_folder` from a different bookmark storage. That is, either `node` is
// local-only and `target_folder` is in the signed-in account, or the other way
// around. This is meant for flows where the user explicitly selected a
// `target_folder`/`index` (e.g. drag and drop) and the dialog text will reflect
// this. See also ShowBookmarkAccountStorageUploadDialog() for cases where the
// user wants to upload a bookmark but the destination is implicit.
//
// `closed_callback` will be invoked once the dialog is closed, either
// because the user accepted the move or canceled it (current callers don't need
// to distinguish the 2 cases). If the user accepts, `node` will become the
// `index`-th child of `target_folder`.
// Must only be called if account bookmarks are enabled - see BookmarkModel::
// account_bookmark_bar_node() for details - and `node` is in a different
// storage from `target_folder`. Otherwise, the call will crash.
// Note: In incognito mode, the dialog will be shown on top of a browser for the
// Original Profile instead of `browser`. It will create a new browser window if
// one doesn't exist already.
void ShowBookmarkAccountStorageMoveDialog(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* target_folder,
    size_t index,
    base::OnceClosure closed_callback = base::DoNothing());

// While ShowBookmarkAccountStorageMoveDialog() above should be used for
// local->account and account->local moves to specific folders, this API should
// be used when the user wants to upload a local-only `node` without specifying
// a target folder/index. In that case, the implicit target will be the end of
// the corresponding account top-level folder. For instance, if `node` is in the
// the local "bookmarks bar", this dialog will move it to the end of the account
// "bookmarks bar". The text is also different from the other function.
//
// `closed_callback` will be invoked once the dialog is closed, either
// because the user accepted the move or canceled it (current callers don't need
// to distinguish the 2 cases).
// Must only be called if account bookmarks are enabled - see BookmarkModel::
// account_bookmark_bar_node() for details - and `node` is local. Otherwise, the
// call will crash.
// Note: In incognito mode, the dialog will be shown on top of a browser for the
// Original Profile instead of `browser`. It will create a new browser window if
// one doesn't exist already.
void ShowBookmarkAccountStorageUploadDialog(
    Browser* browser,
    const bookmarks::BookmarkNode* node,
    base::OnceClosure closed_callback = base::DoNothing());

// Exposed for testing.
// These values are persisted to UMA. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(BookmarkAccountStorageMoveDialogType)
enum class BookmarkAccountStorageMoveDialogType {
  // Corresponds to ShowBookmarkAccountStorageMoveDialog().
  kDownloadOrUpload = 0,
  // Corresponds to ShowBookmarkAccountStorageUploadDialog().
  kUpload = 1,
  kMaxValue = kUpload,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:BookmarkAccountStorageMoveDialogType)

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_ACCOUNT_STORAGE_MOVE_DIALOG_H_
