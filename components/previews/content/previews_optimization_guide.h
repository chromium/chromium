// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/previews/core/previews_experiments.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;

namespace previews {
class PreviewsUserData;

// A Previews optimization guide that makes decisions guided by hints received
// from the OptimizationGuideService.
class PreviewsOptimizationGuide {
 public:
  PreviewsOptimizationGuide() {}
  virtual ~PreviewsOptimizationGuide() {}

  // Returns whether |type| is allowed for the URL associated with
  // |navigation_handle| and the current conditions. |previews_data| can be
  // modified (for further details provided by hints). Note that this will
  // return false if a hint is needed to determine if the preview is allowed but
  // we do not have everything we need to make that determination in memory.
  virtual bool CanApplyPreview(PreviewsUserData* previews_data,
                               content::NavigationHandle* navigation_handle,
                               PreviewsType type) = 0;

  // Returns whether |navigation_handle| may have associated optimization hints
  // (specifically, PageHints). If so, but the hints are not available
  // synchronously, this method will request that they be loaded (from disk or
  // network). The callback is run after the hint is loaded and can be used as
  // a signal during tests.
  virtual bool MaybeLoadOptimizationHints(
      content::NavigationHandle* navigation_handle,
      base::OnceClosure callback) = 0;

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  virtual bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) = 0;
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_OPTIMIZATION_GUIDE_H_
