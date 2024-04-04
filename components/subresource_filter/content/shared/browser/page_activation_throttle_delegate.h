// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_ACTIVATION_THROTTLE_DELEGATE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_ACTIVATION_THROTTLE_DELEGATE_H_

#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "content/public/browser/navigation_handle.h"

namespace subresource_filter {

// Interface that allows the client of a page activation throttle, like
// `SafeBrowsingPageActivationThrottle` to adjust activation decisions if/as
// desired.
class PageActivationThrottleDelegate {
 public:
  virtual ~PageActivationThrottleDelegate() = default;

  // Called when the initial activation decision has been computed by the
  // page activation throttle. Returns the effective activation for this
  // navigation.
  //
  // Note: |decision| is guaranteed to be non-nullptr, and can be modified by
  // this method if any decision changes.
  //
  // Precondition: The navigation must be a root frame navigation.
  virtual mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      mojom::ActivationLevel initial_activation_level,
      ActivationDecision* decision) = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_PAGE_ACTIVATION_THROTTLE_DELEGATE_H_
