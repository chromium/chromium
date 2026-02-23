// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_CONSTANTS_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_CONSTANTS_H_

#include <stdint.h>

#include "base/files/file_path.h"

namespace bookmarks {

extern const base::FilePath::CharType kLocalOrSyncableBookmarksFileName[];
extern const base::FilePath::CharType kAccountBookmarksFileName[];
extern const base::FilePath::CharType
    kEncryptedLocalOrSyncableBookmarksFileName[];
extern const base::FilePath::CharType kEncryptedAccountBookmarksFileName[];

extern const int64_t kRootNodeId;

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_CONSTANTS_H_
