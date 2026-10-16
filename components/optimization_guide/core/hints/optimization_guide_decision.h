// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_DECISION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_DECISION_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "url/gurl.h"

namespace optimization_guide {

// Represents the decision made by the optimization guide.
// Keep in sync with OptimizationGuideOptimizationGuideDecision in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.optimization_guide
enum class OptimizationGuideDecision {
  // The necessary information to make the decision is not yet available.
  kUnknown,
  // The necessary information to make the decision is available and the
  // decision is affirmative.
  kTrue,
  // The necessary information to make the decision is available and the
  // decision is negative.
  kFalse,

  // New values above this line.
  kMaxValue = kFalse,
};

// The types of decisions that can be made for an optimization type.
//
// Keep in sync with OptimizationGuideOptimizationTypeDecision in enums.xml.
enum class OptimizationTypeDecision {
  kUnknown = 0,
  // The optimization type was allowed for the page load by an optimization
  // filter for the type.
  kAllowedByOptimizationFilter = 1,
  // The optimization type was not allowed for the page load by an optimization
  // filter for the type.
  kNotAllowedByOptimizationFilter = 2,
  // An optimization filter for that type was on the device but was not loaded
  // in time to make a decision. There is no guarantee that had the filter been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadOptimizationFilterButNotLoadedInTime = 3,
  // The optimization type was allowed for the page load based on a hint.
  kAllowedByHint = 4,
  // A hint that matched the page load was present but the optimization type was
  // not allowed to be applied.
  kNotAllowedByHint = 5,
  // A hint was available but there was not a page hint within that hint that
  // matched the page load.
  kNoMatchingPageHint = 6,
  // A hint that matched the page load was on the device but was not loaded in
  // time to make a decision. There is no guarantee that had the hint been
  // loaded that the page load would have been allowed for the optimization
  // type.
  kHadHintButNotLoadedInTime = 7,
  // No hints were available in the cache that matched the page load.
  kNoHintAvailable = 8,
  // The OptimizationGuideDecider was not initialized yet.
  kDeciderNotInitialized = 9,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but was not available in time to make a
  // decision.
  kHintFetchStartedButNotAvailableInTime = 10,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but requested optimization type was not
  // registered.
  kRequestedUnregisteredType = 11,
  // A fetch to get the hint for the page load from the remote Optimization
  // Guide Service was started, but requested URL was invalid.
  kInvalidURL = 12,

  // Add new values above this line.
  kMaxValue = kInvalidURL,
};

using OptimizationGuideDecisionCallback =
    base::OnceCallback<void(OptimizationGuideDecision,
                            const OptimizationMetadata&)>;

struct OptimizationGuideDecisionWithMetadata {
  // The decision made by the optimization guide.
  OptimizationGuideDecision decision;
  // The metadata for the optimization type, if applicable.
  OptimizationMetadata metadata;
};

using OnDemandOptimizationGuideDecisionRepeatingCallback =
    base::RepeatingCallback<void(
        const GURL&,
        const base::flat_map<proto::OptimizationType,
                             OptimizationGuideDecisionWithMetadata>&)>;

// Returns a debug string for OptimizationGuideDecision.
std::string_view GetStringForOptimizationGuideDecision(
    OptimizationGuideDecision decision);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_DECISION_H_
