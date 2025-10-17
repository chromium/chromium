// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_

#include <limits.h>

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace history {

// Organic Repeatable Queries
COMPONENT_EXPORT(HISTORY_FEATURES)
BASE_DECLARE_FEATURE(kOrganicRepeatableQueries);
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<int> kMaxNumRepeatableQueries;
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<bool> kScaleRepeatableQueriesScores;
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<bool> kPrivilegeRepeatableQueries;
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<bool> kRepeatableQueriesIgnoreDuplicateVisits;
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<int> kRepeatableQueriesMaxAgeDays;
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<int> kRepeatableQueriesMinVisitCount;

// When enabled, this feature flag begins populating the VisitedLinkDatabase
// with data.
COMPONENT_EXPORT(HISTORY_FEATURES)
BASE_DECLARE_FEATURE(kPopulateVisitedLinkDatabase);

COMPONENT_EXPORT(HISTORY_FEATURES) BASE_DECLARE_FEATURE(kVisitedLinksOn404);

// Most Visited Tiles scoring function changes.
COMPONENT_EXPORT(HISTORY_FEATURES)
BASE_DECLARE_FEATURE(kMostVisitedTilesNewScoring);

// List of values for |kMvtScoringParamRecencyFactor|
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const char kMvtScoringParamRecencyFactor_Classic[];
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const char kMvtScoringParamRecencyFactor_Decay[];
COMPONENT_EXPORT(HISTORY_FEATURES)
extern const char kMvtScoringParamRecencyFactor_DecayStaircase[];

COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<std::string> kMvtScoringParamRecencyFactor;

COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<double> kMvtScoringParamDecayPerDay;

COMPONENT_EXPORT(HISTORY_FEATURES)
extern const base::FeatureParam<int> kMvtScoringParamDailyVisitCountCap;

COMPONENT_EXPORT(HISTORY_FEATURES)
BASE_DECLARE_FEATURE(kRazeOldHistoryDatabase);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(HISTORY_FEATURES)
BASE_DECLARE_FEATURE(kBrowsingHistoryActorIntegrationM2);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_FEATURES_H_
