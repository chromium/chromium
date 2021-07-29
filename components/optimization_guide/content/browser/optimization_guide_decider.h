// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_GUIDE_DECIDER_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;

namespace optimization_guide {

class OptimizationGuideDecider {
 public:
  // Registers the optimization targets that intend to be queried during the
  // session. It is expected for this to be called after the browser has been
  // initialized.
  virtual void RegisterOptimizationTargets(
      const std::vector<proto::OptimizationTarget>& optimization_targets) = 0;

  // Invokes |callback| with the decision for whether the current browser
  // conditions, as expressed by |client_model_feature_values| and the
  // |navigation_handle|, match |optimization_target|.
  //
  // Values provided in |client_model_feature_values| will be used over any
  // values for features required by the model that may be calculated by the
  // Optimization Guide.
  virtual void ShouldTargetNavigationAsync(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationTarget optimization_target,
      OptimizationGuideTargetDecisionCallback callback) = 0;

  // Registers the optimization types that intend to be queried during the
  // session. It is expected for this to be called after the browser has been
  // initialized.
  virtual void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types) = 0;

  // Invokes |callback| with the decision for the URL contained in
  // |navigation_handle| and |optimization_type|, when sufficient information
  // has been collected to make the decision. This should only be called for
  // main frame navigations.
  virtual void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) = 0;

  // Returns whether |optimization_type| can be applied for |url|. This should
  // only be called for main frame navigations or future main frame navigations.
  //
  // Note: DO NOT USE this method if you intend to opt into the Optimization
  // Guide's autotuning framework at any point.
  virtual OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) = 0;

 protected:
  OptimizationGuideDecider() {}
  virtual ~OptimizationGuideDecider() {}
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_GUIDE_DECIDER_H_
