// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_MANAGER_H_

#include "base/observer_list.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
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
// !!!WARNING!!!: This observer will receive notifications from all pages
// within a WebContents. This includes non-primary pages like those that are
// prerendering which is probably not what clients expect. Clients should
// make sure they're manually scoping observations to the relevant page.
// TODO(bokan): We should probably refactor this class to manage the
// observations of a single Page/FrameTree. #MPArch
class SubresourceFilterObserverManager
    : public content::WebContentsUserData<SubresourceFilterObserverManager> {
 public:
  explicit SubresourceFilterObserverManager(content::WebContents* web_contents);

  SubresourceFilterObserverManager(const SubresourceFilterObserverManager&) =
      delete;
  SubresourceFilterObserverManager& operator=(
      const SubresourceFilterObserverManager&) = delete;

  ~SubresourceFilterObserverManager() override;

  void AddObserver(SubresourceFilterObserver* observer);
  void RemoveObserver(SubresourceFilterObserver* observer);

  // Called when the SubresourceFilter Safe Browsing checks are available for
  // this root frame navigation. Will be called at WillProcessResponse time at
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
  // ChildFrameNavigationFilteringThrottle.
  void NotifyChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy);

  // Called in DidCreateNewDocument or ReadyToCommitNavigation to notify
  // observers that an frame with the associated RenderFrameHost has either been
  // detected as an ad or is no longer considered one. The frame's new status is
  // passed as `is_ad_frame`.
  void NotifyIsAdFrameChanged(content::RenderFrameHost* render_frame_host,
                              bool is_ad_frame);

 private:
  friend class content::WebContentsUserData<SubresourceFilterObserverManager>;
  base::ObserverList<SubresourceFilterObserver>::Unchecked observers_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_MANAGER_H_
