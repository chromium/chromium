// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_features.h"

#include "base/feature_list.h"

namespace bookmarks {

// If enabled, uses an approximate pre-check to determine if an input matches a
// particular bookmark index node. This pre-check is faster than the more
// accurate check, but it returns false positives; therefore, it's only a
// precursor to and not a replacement for the real check. Does nothing if
// `omnibox::kBookmarkPaths` is disabled.
const base::Feature kApproximateNodeMatch{"BookmarkApproximateNodeMatch",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace bookmarks
