// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_DECISION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_DECISION_H_

#include "base/callback_forward.h"
#include "components/optimization_guide/core/optimization_metadata.h"

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
    base::OnceCallback<void(optimization_guide::OptimizationGuideDecision,
                            const optimization_guide::OptimizationMetadata&)>;

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_DECISION_H_
