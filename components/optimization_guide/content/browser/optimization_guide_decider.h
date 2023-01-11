// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_GUIDE_DECIDER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;

namespace optimization_guide {

class OptimizationGuideDecider {
 public:
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
  virtual OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) = 0;

 protected:
  OptimizationGuideDecider() = default;
  virtual ~OptimizationGuideDecider() = default;

 private:
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

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_OPTIMIZATION_GUIDE_DECIDER_H_
