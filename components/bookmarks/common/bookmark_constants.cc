// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_constants.h"

#define FPL FILE_PATH_LITERAL

namespace bookmarks {

// The actual file name is inconsistent with variable name for historical
// reasons and kept as is to avoid risky migrations for existing users.
const base::FilePath::CharType kLocalOrSyncableBookmarksFileName[] =
    FPL("Bookmarks");
const base::FilePath::CharType kAccountBookmarksFileName[] =
    FPL("AccountBookmarks");
// TODO(crbug.com/435317726): There was a rollback at one point and the files:
// EncryptedBookmarks and EncryptedAccountBookmarks were abandoned. When the
// code is cleaned up those files should be cleaned up.
const base::FilePath::CharType kEncryptedLocalOrSyncableBookmarksFileName[] =
    FPL("EncryptedBookmarks2");
const base::FilePath::CharType kEncryptedAccountBookmarksFileName[] =
    FPL("EncryptedAccountBookmarks2");

// ID of the root node. This is also exposed externally via an extensions API.
const int64_t kRootNodeId = 0;

}  // namespace bookmarks
