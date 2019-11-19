// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_

#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/load_policy.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

namespace mojom {
class ActivationState;
}  // namespace mojom

// Class to receive notifications of subresource filter events for a given
// WebContents. Registered with a SubresourceFilterObserverManager.
class SubresourceFilterObserver {
 public:
  virtual ~SubresourceFilterObserver() = default;

  // Called before the observer manager is destroyed. Observers must unregister
  // themselves by this point.
  virtual void OnSubresourceFilterGoingAway() {}

  // The results from a set of safe browsing checks, stored as a vector.
  using SafeBrowsingCheckResults =
      std::vector<SubresourceFilterSafeBrowsingClient::CheckResult>;

  // Called when the SubresourceFilter Safe Browsing checks are available for
  // this main frame navigation. Will be called at WillProcessResponse time at
  // the latest. Right now it will only include phishing and subresource filter
  // threat types.
  virtual void OnSafeBrowsingChecksComplete(
      content::NavigationHandle* navigation_handle,
      const SafeBrowsingCheckResults& results) {}

  // Called at most once per navigation when page activation is computed. This
  // will be called before ReadyToCommitNavigation.
  virtual void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationState& activation_state) {}

  // Called before navigation commit, either at the WillStartRequest stage or
  // WillRedirectRequest stage. |is_ad_subframe| is true if |load_policy| is
  // ALLOW or WOULD_DISALLOW or if ad tagging has determined that the frame is
  // an ad.
  virtual void OnSubframeNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy,
      bool is_ad_subframe) {}

  // Use this to detect ad frames that never navigate beyond about:blank (e.g.,
  // doc.written frames). Called when a frame is first created and navigated to
  // about:blank and its already known that the frame is an ad. E.g., because
  // the script that created the frame is ad script or the frame is a child of
  // an ad frame.
  virtual void OnAdSubframeDetected(
      content::RenderFrameHost* render_frame_host) {}
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_
