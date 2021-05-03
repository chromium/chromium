// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/features/features.h"

#include "base/feature_list.h"

namespace federated_learning {

// Enables or disables the FlocIdComputed event logging, which happens when a
// floc id is first computed for a browsing session or is refreshed due to a
// long period of time has passed since the last computation.
const base::Feature kFlocIdComputedEventLogging{
    "FlocIdComputedEventLogging", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the sim-hash floc computed from history will be further encoded
// based on the sorting-lsh.
const base::Feature kFlocIdSortingLshBasedComputation{
    "FlocIdSortingLshBasedComputation", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, pages that had ad resources will be included in floc computation;
// otherwise, only pages that used the document.interestCohort API will be
// included. This flag affects a bit to be stored at page viewing time, so it
// may take a full computation cycle for the floc to meet the configured
// criteria.
const base::Feature kFlocPagesWithAdResourcesDefaultIncludedInFlocComputation{
    "FlocPagesWithAdResourcesDefaultIncludedInFlocComputation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// The main floc feature for all the subsidiary control and setting params. It's
// controlling the floc update rate, and the minimum history domain size
// required.
// TODO(yaoxia): merge other floc features into this one.
const base::Feature kFederatedLearningOfCohorts{
    "FederatedLearningOfCohorts", base::FEATURE_ENABLED_BY_DEFAULT};
constexpr base::FeatureParam<base::TimeDelta> kFlocIdScheduledUpdateInterval{
    &kFederatedLearningOfCohorts, "update_interval",
    base::TimeDelta::FromDays(7)};
constexpr base::FeatureParam<int> kFlocIdMinimumHistoryDomainSizeRequired{
    &kFederatedLearningOfCohorts, "minimum_history_domain_size_required", 3};
constexpr base::FeatureParam<int> kFlocIdFinchConfigVersion{
    &kFederatedLearningOfCohorts, "finch_config_version", 1};

}  // namespace federated_learning
