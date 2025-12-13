// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/feature_list.h"

namespace bookmarks {

// This feature controls the default visibility for permanent folders when
// empty. It effectively swaps in "other bookmarks" as the default-visible
// empty folder on mobile. This flag has no effect for desktop.
BASE_FEATURE(kAllBookmarksBaselineFolderVisibility,
             base::FEATURE_DISABLED_BY_DEFAULT);

// This feature enables/disables using a SHA256 checksum alongside an
// md5 checksum. It is part of the MD5 deprecation efforts and is expected
// to be removed.
BASE_FEATURE(kEnableBookmarkCodecSHA256, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace bookmarks
