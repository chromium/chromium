// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace bookmarks {

extern const base::Feature kApproximateNodeMatch;

extern const base::Feature kTypedUrlsMap;

extern const base::Feature kLimitNumNodesForBookmarkSearch;

extern const base::FeatureParam<int> kLimitNumNodesForBookmarkSearchCount;

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_FEATURES_H_
