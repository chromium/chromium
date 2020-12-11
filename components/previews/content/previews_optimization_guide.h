// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;

namespace optimization_guide {
class OptimizationGuideDecider;
enum class OptimizationGuideDecision;
}  // namespace optimization_guide

namespace previews {
enum class PreviewsType;
class PreviewsUserData;

// A Previews optimization guide that makes decisions guided by hints received
// from an |optimization_guide::OptimizationGuideDecider|.
class PreviewsOptimizationGuide {
 public:
  explicit PreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  PreviewsOptimizationGuide(const PreviewsOptimizationGuide&) = delete;
  PreviewsOptimizationGuide& operator=(const PreviewsOptimizationGuide&) =
      delete;
  virtual ~PreviewsOptimizationGuide();

  // Kicks off the call to |optimization_guide_decider_| for whether a Preview
  // should be shown for the current conditions.
  virtual void StartCheckingIfShouldShowPreview(
      content::NavigationHandle* navigation_handle);

  // Returns whether a Preview should be shown for the current conditions.
  virtual bool ShouldShowPreview(content::NavigationHandle* navigation_handle);

  // Returns whether |type| is allowed for the URL associated with
  // |navigation_handle|. |previews_data| can be
  // modified (for further details provided by hints).
  virtual bool CanApplyPreview(PreviewsUserData* previews_data,
                               content::NavigationHandle* navigation_handle,
                               PreviewsType type);

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  virtual bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block);

 private:
  // Invoked when |optimization_guide_decider_| sends the decision for whether
  // the page load for the navigation, as expressed by |navigation_id|, is
  // painful or not.
  void OnPainfulPageLoadDecision(
      int64_t navigation_id,
      optimization_guide::OptimizationGuideDecision decision);

  // The Optimization Guide Decider to consult for whether an optimization can
  // be applied. Not owned.
  optimization_guide::OptimizationGuideDecider* optimization_guide_decider_;

  // An in-memory cache of resource loading hints keyed by the URL. This allows
  // us to avoid making too many calls to |optimization_guide_decider_|.
  base::MRUCache<GURL, std::vector<std::string>> resource_loading_hints_cache_;

  // An in-memory cache of painful page load decisions keyed by the navigation
  // ID of the navigation handle that the decision was evaluated on.
  base::MRUCache<int64_t, optimization_guide::OptimizationGuideDecision>
      painful_page_load_decisions_;

  // The optimization types registered with |optimization_guide_decider_|.
  const base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  base::WeakPtrFactory<PreviewsOptimizationGuide> weak_ptr_factory_{this};
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
