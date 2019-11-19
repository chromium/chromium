// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_DECIDER_H_

#include "components/previews/content/previews_optimization_guide.h"

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace previews {

// An |optimization_guide::OptimizationDecider| backed implementation of
// |PreviewsOptimizationGuide|.
class PreviewsOptimizationGuideDecider : public PreviewsOptimizationGuide {
 public:
  explicit PreviewsOptimizationGuideDecider(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  ~PreviewsOptimizationGuideDecider() override;

  // PreviewsOptimizationGuide implementation:
  bool CanApplyPreview(PreviewsUserData* previews_data,
                       content::NavigationHandle* navigation_handle,
                       PreviewsType type) override;
  bool MaybeLoadOptimizationHints(content::NavigationHandle* navigation_handle,
                                  base::OnceClosure callback) override;
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) override;

 private:
  // The Optimization Guide Decider to consult for whether an optimization can
  // be applied. Not owned.
  optimization_guide::OptimizationGuideDecider* optimization_guide_decider_;

  // An in-memory cache of resource loading hints keyed by the URL. This allows
  // us to avoid making too many calls to |optimization_guide_decider_|.
  base::MRUCache<GURL, std::vector<std::string>> resource_loading_hints_cache_;

  // The optimization types registered with |optimization_guide_decider_|.
  const base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuideDecider);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_DECIDER_H_
