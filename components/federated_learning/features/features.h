// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FEATURES_FEATURES_H_
#define COMPONENTS_FEDERATED_LEARNING_FEATURES_FEATURES_H_

#include "base/feature_list.h"

namespace federated_learning {

extern const base::Feature kFlocIdComputedEventLogging;

extern const base::Feature kFlocIdSortingLshBasedComputation;

extern const base::Feature
    kFlocPagesWithAdResourcesDefaultIncludedInFlocComputation;

extern const base::Feature kFederatedLearningOfCohorts;
extern const base::FeatureParam<base::TimeDelta> kFlocIdScheduledUpdateInterval;
extern const base::FeatureParam<int> kFlocIdMinimumHistoryDomainSizeRequired;
extern const base::FeatureParam<int> kFlocIdFinchConfigVersion;

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FEATURES_FEATURES_H_