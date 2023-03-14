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

}  // namespace bookmarks
