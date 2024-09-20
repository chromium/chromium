// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history {

// Organic Repeatable Queries
BASE_DECLARE_FEATURE(kOrganicRepeatableQueries);
extern const base::FeatureParam<int> kMaxNumRepeatableQueries;
extern const base::FeatureParam<bool> kScaleRepeatableQueriesScores;
extern const base::FeatureParam<bool> kPrivilegeRepeatableQueries;
extern const base::FeatureParam<bool> kRepeatableQueriesIgnoreDuplicateVisits;
extern const base::FeatureParam<int> kRepeatableQueriesMaxAgeDays;
extern const base::FeatureParam<int> kRepeatableQueriesMinVisitCount;

// When enabled, this feature flag begins populating the VisitedLinkDatabase
// with data.
BASE_DECLARE_FEATURE(kPopulateVisitedLinkDatabase);

// If enabled, change the scoring function for most visited tiles.
BASE_DECLARE_FEATURE(kMostVisitedTilesNewScoring);

// |kMostVisitedTilesNewScoring|: Feature param names.
extern const char kMvtScoringParamRecencyFactor[];
extern const char kMvtScoringParamDailyVisitCountCap[];

// |kMvtScoringParamRecencyFactor|: Feature param values.
extern const char kMvtScoringParamRecencyFactor_Default[];
extern const char kMvtScoringParamRecencyFactor_DecayStaircase[];

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
