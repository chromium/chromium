// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_MOVE_BOOKMARK_TO_ACCOUNT_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_MOVE_BOOKMARK_TO_ACCOUNT_DIALOG_H_

class Browser;

// Shows a modal dialog in `browser`'s window that offers to move a bookmark
// from local-only storage to the signed-in account. Must only be called if
// there is a signed-in account.
// TODO(crbug.com/354896249): Take a dismissal callback as argument.
void ShowMoveBookmarkToAccountDialog(Browser* browser);

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_MOVE_BOOKMARK_TO_ACCOUNT_DIALOG_H_
