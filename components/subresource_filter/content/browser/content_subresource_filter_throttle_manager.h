// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/mojom/subresource_filter.mojom.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;
class ActivationStateComputingNavigationThrottle;
class PageLoadStatistics;
class SubresourceFilterObserverManager;
class SubresourceFilterClient;

// The ContentSubresourceFilterThrottleManager manages NavigationThrottles in
// order to calculate frame activation states and subframe navigation filtering,
// within a given WebContents. It contains a mapping of all activated
// RenderFrameHosts, along with their associated DocumentSubresourceFilters.
//
// The class is designed to be used by a Delegate, which shares lifetime with
// this class (aka the typical lifetime of a WebContentsObserver). The delegate
// will be notified of the first disallowed subresource load for a top level
// navgation, and has veto power for frame activation.
class ContentSubresourceFilterThrottleManager
    : public content::WebContentsObserver,
      public mojom::SubresourceFilterHost,
      public SubresourceFilterObserver,
      public SubframeNavigationFilteringThrottle::Delegate {
 public:
  ContentSubresourceFilterThrottleManager(
      SubresourceFilterClient* client,
      VerifiedRulesetDealer::Handle* dealer_handle,
      content::WebContents* web_contents);
  ~ContentSubresourceFilterThrottleManager() override;

  // This method inspects |navigation_handle| and attaches navigation throttles
  // appropriately, based on the current state of frame activation.
  //
  // 1. Subframe navigation filtering throttles are appended if the parent
  // frame is activated.
  // 2. Activation state computing throttles are appended if either the
  // navigation is a main frame navigation, or if the parent frame is activated.
  //
  // Note that there is currently no constraints on the ordering of throttles.
  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles);

  PageLoadStatistics* page_load_statistics() const { return statistics_.get(); }

  VerifiedRuleset::Handle* ruleset_handle_for_testing() {
    return ruleset_handle_.get();
  }

  // SubframeNavigationFilteringThrottle::Delegate:
  bool CalculateIsAdSubframe(content::RenderFrameHost* frame_host,
                             LoadPolicy load_policy) override;

  bool IsFrameTaggedAsAdForTesting(content::RenderFrameHost* frame_host) const;

 protected:
  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* frame_host) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // SubresourceFilterObserver:
  void OnSubresourceFilterGoingAway() override;
  void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationState& activation_state) override;
  void OnSubframeNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy,
      bool is_ad_subframe) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           SubframeNavigationTaggedAsAdByRenderer);
  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           GrandchildNavigationTaggedAsAdByRenderer);
  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           AdTagCarriesAcrossProcesses);
  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           FirstDisallowedLoadCalledOutOfOrder);
  std::unique_ptr<SubframeNavigationFilteringThrottle>
  MaybeCreateSubframeNavigationFilteringThrottle(
      content::NavigationHandle* navigation_handle);
  std::unique_ptr<ActivationStateComputingNavigationThrottle>
  MaybeCreateActivationStateComputingThrottle(
      content::NavigationHandle* navigation_handle);

  // Will return nullptr if the parent frame of this navigation is not
  // activated (and therefore has no subresource filter).
  AsyncDocumentSubresourceFilter* GetParentFrameFilter(
      content::NavigationHandle* child_frame_navigation);

  // Calls ShowNotification on |client_| at most once per committed,
  // non-same-page navigation in the main frame.
  void MaybeShowNotification();

  VerifiedRuleset::Handle* EnsureRulesetHandle();
  void DestroyRulesetHandleIfNoLongerUsed();

  // Registers |render_frame_host| as an ad frame. If the frame later moves to
  // a new process its RenderHost will be told that it's an ad.
  void OnFrameIsAdSubframe(content::RenderFrameHost* render_frame_host);

  // mojom::SubresourceFilterHost:
  void DidDisallowFirstSubresource() override;
  void FrameIsAdSubframe() override;
  void SetDocumentLoadStatistics(
      mojom::DocumentLoadStatisticsPtr statistics) override;

  // Adds the navigation's RenderFrameHost to activated_frame_hosts_ if it is a
  // special navigation which did not go through navigation throttles and its
  // parent frame is activated as well. The filter for these frames is set
  // to nullptr.
  void MaybeActivateSubframeSpecialUrls(
      content::NavigationHandle* navigation_handle);

  // For each RenderFrameHost where the last committed load has subresource
  // filtering activated, owns the corresponding AsyncDocumentSubresourceFilter.
  // It is possible for a frame to have a null filter.
  std::map<content::RenderFrameHost*,
           std::unique_ptr<AsyncDocumentSubresourceFilter>>
      activated_frame_hosts_;

  // For each ongoing navigation that requires activation state computation,
  // keeps track of the throttle that is carrying out that computation, so that
  // the result can be retrieved when the navigation is ready to commit.
  std::map<content::NavigationHandle*,
           ActivationStateComputingNavigationThrottle*>
      ongoing_activation_throttles_;

  // Set of RenderFrameHosts that have been identified as ads. An RFH is an ad
  // subframe if any of the following conditions are met:
  // 1. Its navigation URL is in the filter list
  // 2. Its parent is a known ad subframe
  // 3. The RenderFrame declares the frame is an ad (see AdTracker in Blink)
  // 4. It's the result of moving an old ad subframe RFH to a new RFH (e.g.,
  //    OOPIF)
  std::set<content::RenderFrameHost*> ad_frames_;

  content::WebContentsFrameBindingSet<mojom::SubresourceFilterHost> binding_;

  ScopedObserver<SubresourceFilterObserverManager, SubresourceFilterObserver>
      scoped_observer_;

  // Lazily instantiated in EnsureRulesetHandle when the first page level
  // activation is triggered. Will go away when there are no more activated
  // RenderFrameHosts (i.e. activated_frame_hosts_ is empty).
  std::unique_ptr<VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<PageLoadStatistics> statistics_;

  // True if the current committed main frame load in this WebContents has
  // notified the delegate that a subresource was disallowed. The callback
  // should only be called at most once per main frame load.
  bool current_committed_load_has_notified_disallowed_load_ = false;

  // These members outlive this class.
  VerifiedRulesetDealer::Handle* dealer_handle_;
  SubresourceFilterClient* client_;

  base::WeakPtrFactory<ContentSubresourceFilterThrottleManager>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterThrottleManager);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_
