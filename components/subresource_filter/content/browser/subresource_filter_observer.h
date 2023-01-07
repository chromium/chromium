// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_

#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
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
// !!!WARNING!!!: This observer will receive notifications from all pages
// within a WebContents. This includes non-primary pages like those that are
// prerendering which is probably not what clients expect. Clients should
// make sure they're manually scoping observations to the relevant page.
// TODO(bokan): We should probably refactor this and the manager class
// to register against a single Page/FrameTree. #MPArch
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
  // this root frame navigation. Will be called at WillProcessResponse time at
  // the latest. Right now it will only include phishing and subresource filter
  // threat types.
  virtual void OnSafeBrowsingChecksComplete(
      content::NavigationHandle* navigation_handle,
      const SubresourceFilterSafeBrowsingClient::CheckResult& result) {}

  // Called at most once per navigation when page activation is computed. This
  // will be called before ReadyToCommitNavigation.
  virtual void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationState& activation_state) {}

  // Called before navigation commit, either at the WillStartRequest stage or
  // WillRedirectRequest stage.
  virtual void OnChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy) {}

  // Called when a frame is tagged or untagged as an ad, along with the frame's
  // current status as an ad frame and the evidence which resulted in the
  // change. This will be called prior to commit time in the case of an initial
  // synchronous load or at ReadyToCommitNavigation otherwise.
  virtual void OnIsAdFrameChanged(content::RenderFrameHost* render_frame_host,
                                  bool is_ad_frame) {}
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_H_
