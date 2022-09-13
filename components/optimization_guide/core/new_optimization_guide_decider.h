// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_NEW_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_NEW_OPTIMIZATION_GUIDE_DECIDER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"

class GURL;

namespace commerce {
class ShoppingService;
}  // namespace commerce

namespace optimization_guide {

// WARNING: This class is not quite ready for use yet -- use
// OptimizationGuideDecider instead.
//
// This class is eventually intended to replace the OptimizationGuideDecider
// as an interface that can be used in the "core" directory of other components
// (lacking any platform-sepcific dependencies).
class NewOptimizationGuideDecider {
 public:
  // Registers the optimization types that intend to be queried during the
  // session. It is expected for this to be called after the browser has been
  // initialized.
  virtual void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types) = 0;

  // Invokes |callback| with the decision for the URL contained in |url| and
  // |optimization_type|, when sufficient information has been collected to
  // make the decision.
  virtual void CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) = 0;

  // Returns whether |optimization_type| can be applied for |url|. This should
  // only be called for main frame navigations or future main frame navigations.
  virtual OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) = 0;

 protected:
  NewOptimizationGuideDecider() = default;
  virtual ~NewOptimizationGuideDecider() = default;

 private:
  // ShoppingService is a friend class since it is a consumer of the
  // CanApplyOptimizationOnDemand API.
  friend class commerce::ShoppingService;

  // Invokes |callback| with the decision for all types contained in
  // |optimization_types| for each URL contained in |urls|, when sufficient
  // information has been collected to make decisions. |request_context| must be
  // included to indicate when the request is being made to determine the
  // appropriate permissions to make the request for accounting purposes.
  // Consumers must call `RegisterOptimizationTypes` once during the session
  // before calling this method.
  //
  // It is expected for consumers to consult with the Optimization Guide team
  // before using this API. If approved, add your class as a friend class here.
  virtual void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<proto::OptimizationType>& optimization_types,
      proto::RequestContext request_context,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_NEW_OPTIMIZATION_GUIDE_DECIDER_H_
