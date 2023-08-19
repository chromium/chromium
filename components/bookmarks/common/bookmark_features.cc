// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/feature_list.h"

namespace bookmarks {

// This feature flags enables the logic that wipes the account storage after
// EnableBookmarksAccountStorage rollback. This logic is not enabled by default
// to minimize the performance impact.
BASE_FEATURE(kRollbackBookmarksAccountStorage,
             "RollbackBookmarksAccountStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace bookmarks
