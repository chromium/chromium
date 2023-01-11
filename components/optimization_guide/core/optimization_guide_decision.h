// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_DECISION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_DECISION_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/optimization_metadata.h"
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

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_DECISION_H_
