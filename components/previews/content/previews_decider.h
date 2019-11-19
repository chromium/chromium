// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_DECIDER_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_DECIDER_H_

#include <string>
#include <vector>

#include "components/previews/core/previews_experiments.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;

namespace previews {
class PreviewsUserData;

class PreviewsDecider {
 public:
  // Whether the URL for |navigation_handle| is allowed to show a preview of
  // |type| as can be determined at the start of a navigation (or start of a
  // redirection). This can be further checked at navigation commit time via
  // |ShouldCommitPreview|. Some types of previews will be checked for an
  // applicable network quality threshold - these are client previews that do
  // not have optimization hint support. Previews with optimization hint support
  // can have variable network quality thresholds based on the committed URL.
  // Data Reduction Proxy previews (i.e., LITE_PAGE) perform a network quality
  // check on the server.
  virtual bool ShouldAllowPreviewAtNavigationStart(
      PreviewsUserData* previews_data,
      content::NavigationHandle* navigation_handle,
      bool is_reload,
      PreviewsType type) const = 0;

  // Whether the URL for |navigation_handle| is allowed to show a preview of
  // |type|.
  virtual bool ShouldCommitPreview(PreviewsUserData* previews_data,
                                   content::NavigationHandle* navigation_handle,
                                   PreviewsType type) const = 0;

  // Requests that any applicable detailed page hints be loaded. Returns
  // whether client knows that it has hints for the host of the URL for
  // |navigation_handle| (that may need to be loaded from persistent storage).
  virtual bool LoadPageHints(content::NavigationHandle* navigation_handle) = 0;

  // Whether |url| has loaded resource loading hints and, if it does, populates
  // |out_resource_patterns_to_block| with the resource patterns to block.
  virtual bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const = 0;

 protected:
  PreviewsDecider() {}
  virtual ~PreviewsDecider() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_DECIDER_H_
