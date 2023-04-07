// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/feature_list.h"

namespace bookmarks {

// If enabled, there will be two different BookmarkModel instances per profile:
// one instance for "profile" bookmarks and another instance for "account"
// bookmarks. See https://crbug.com/1404250 for details.
BASE_FEATURE(kEnableBookmarksAccountStorage,
             "EnableBookmarksAccountStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace bookmarks
