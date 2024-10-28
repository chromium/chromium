// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_DECIDER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace optimization_guide {

class MockOptimizationGuideDecider : public OptimizationGuideDecider {
 public:
  MockOptimizationGuideDecider();
  MockOptimizationGuideDecider(const MockOptimizationGuideDecider&) = delete;
  MockOptimizationGuideDecider& operator=(const MockOptimizationGuideDecider&) =
      delete;
  ~MockOptimizationGuideDecider() override;

  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               proto::OptimizationType,
               OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&, proto::OptimizationType, OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<proto::OptimizationType>&,
       proto::RequestContext,
       OnDemandOptimizationGuideDecisionRepeatingCallback,
       std::optional<proto::RequestContextMetadata> request_context_metadata),
      (override));
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MOCK_OPTIMIZATION_GUIDE_DECIDER_H_
