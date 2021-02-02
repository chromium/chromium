// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_MANAGER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/common/ad_evidence.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace subresource_filter {

namespace mojom {
class ActivationState;
}  // namespace mojom

// Manages retaining the list of SubresourceFilterObservers and notifying them
// of various filtering events. Scoped to the lifetime of a WebContents.
class SubresourceFilterObserverManager
    : public content::WebContentsUserData<SubresourceFilterObserverManager> {
 public:
  explicit SubresourceFilterObserverManager(content::WebContents* web_contents);
  ~SubresourceFilterObserverManager() override;

  void AddObserver(SubresourceFilterObserver* observer);
  void RemoveObserver(SubresourceFilterObserver* observer);

  // Called when the SubresourceFilter Safe Browsing checks are available for
  // this main frame navigation. Will be called at WillProcessResponse time at
  // the latest. Right now it will only include phishing and subresource filter
  // threat types.
  virtual void NotifySafeBrowsingChecksComplete(
      content::NavigationHandle* navigation_handle,
      const SubresourceFilterSafeBrowsingClient::CheckResult& result);

  // Will be called at the latest in the WillProcessResponse stage from a
  // NavigationThrottle that was registered before the throttle manager's
  // throttles created in MaybeAppendNavigationThrottles().
  void NotifyPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationState& activation_state);

  // Called in WillStartRequest or WillRedirectRequest stage from a
  // SubframeNavigationFilteringThrottle.
  void NotifySubframeNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy);

  // Called in DidCreateNewDocument or ReadyToCommitNavigation to notify
  // observers that an ad frame has been detected with the associated
  // RenderFrameHost. The evidence that caused the frame to be tagged is passed
  // as `ad_evidence`.
  void NotifyAdSubframeDetected(content::RenderFrameHost* render_frame_host,
                                const FrameAdEvidence& ad_evidence);

 private:
  friend class content::WebContentsUserData<SubresourceFilterObserverManager>;
  base::ObserverList<SubresourceFilterObserver>::Unchecked observers_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterObserverManager);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_MANAGER_H_
