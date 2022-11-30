// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/features.h"

#include "components/history/core/browser/top_sites_impl.h"

namespace history {

// If enabled, the most repeated queries from the user browsing history are
// shown in the Most Visited tiles.
BASE_FEATURE(kOrganicRepeatableQueries,
             "OrganicRepeatableQueries",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The maximum number of repeatable queries to show in the Most Visited tiles.
// The default behavior is having no limit, i.e., the number of the tiles.
const base::FeatureParam<int> kMaxNumRepeatableQueries(
    &kOrganicRepeatableQueries,
    "MaxNumRepeatableQueries",
    kTopSitesNumber);

// Whether the scores for the repeatable queries and the most visited sites
// should first be scaled to an equivalent range before mixing.
// The default behavior is to mix the two lists as is.
const base::FeatureParam<bool> kScaleRepeatableQueriesScores(
    &kOrganicRepeatableQueries,
    "ScaleRepeatableQueriesScores",
    false);

// Whether a repeatable query should precede a most visited site with equal
// score. The default behavior is for the sites to precede the queries.
// Used for tie-breaking, especially when kScaleRepeatableQueriesScores is true.
const base::FeatureParam<bool> kPrivilegeRepeatableQueries(
    &kOrganicRepeatableQueries,
    "PrivilegeRepeatableQueries",
    false);

}  // namespace history
