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
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;
class ActivationStateComputingNavigationThrottle;
class PageLoadStatistics;
class SubresourceFilterClient;

// This enum backs a histogram. Make sure new elements are only added to the
// end. Keep histograms.xml up to date with any changes.
enum class SubresourceFilterAction {
  // Standard UI shown. On Desktop this is in the omnibox,
  // On Android, it is an infobar.
  kUIShown = 0,

  // The UI was suppressed due to "smart" logic which tries not to spam the UI
  // on navigations on the same origin within a certain time.
  kUISuppressed = 1,

  // On Desktop, this is a bubble. On Android it is an
  // expanded infobar.
  kDetailsShown = 2,

  kClickedLearnMore = 3,

  // Logged when the user presses "Always allow ads" scoped to a particular
  // site. Does not count manual changes to content settings.
  kAllowlistedSite = 4,

  // Logged when a devtools message arrives notifying us to force activation in
  // this web contents.
  kForcedActivationEnabled = 5,

  kMaxValue = kForcedActivationEnabled
};

// The ContentSubresourceFilterThrottleManager manages NavigationThrottles in
// order to calculate frame activation states and subframe navigation filtering,
// within a given WebContents. It contains a mapping of all activated
// RenderFrameHosts, along with their associated DocumentSubresourceFilters.
//
// The class is designed to be attached to a WebContents instance by an embedder
// via CreateForWebContents(), with the embedder passing a
// SubresourceFilterClient instance customized for that embedder. The client
// will be notified of the first disallowed subresource load for a top level
// navgation, and has veto power for frame activation.
class ContentSubresourceFilterThrottleManager
    : public base::SupportsUserData::Data,
      public content::WebContentsObserver,
      public mojom::SubresourceFilterHost,
      public SubresourceFilterObserver,
      public SubframeNavigationFilteringThrottle::Delegate {
 public:
  static const char
      kContentSubresourceFilterThrottleManagerWebContentsUserDataKey[];

  // Creates a ThrottleManager instance from the given parameters and attaches
  // it as user data of |web_contents|.
  // NOTE: Short-circuits out if the kSafeBrowsingSubresourceFilter feature is
  // not enabled.
  static void CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<SubresourceFilterClient> client,
      VerifiedRulesetDealer::Handle* dealer_handle);

  static ContentSubresourceFilterThrottleManager* FromWebContents(
      content::WebContents* web_contents);
  static const ContentSubresourceFilterThrottleManager* FromWebContents(
      const content::WebContents* web_contents);

  ContentSubresourceFilterThrottleManager(
      std::unique_ptr<SubresourceFilterClient> client,
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

  SubresourceFilterClient* client() { return client_.get(); }

  VerifiedRuleset::Handle* ruleset_handle_for_testing() {
    return ruleset_handle_.get();
  }

  // SubframeNavigationFilteringThrottle::Delegate:
  bool CalculateIsAdSubframe(content::RenderFrameHost* frame_host,
                             LoadPolicy load_policy) override;

  // Returns whether |frame_host| is considered to be an ad.
  bool IsFrameTaggedAsAd(const content::RenderFrameHost* frame_host) const;

  // Returns whether the last navigation resource in |frame_host| was detected
  // to be an ad. A null optional indicates there was no previous navigation or
  // the last navigation was not evaluated by the subresource filter in
  // |frame_host|. Load policy is determined by presence of the navigation url
  // in the filter list.
  base::Optional<LoadPolicy> LoadPolicyForLastCommittedNavigation(
      const content::RenderFrameHost* frame_host) const;

  // Notifies the client that the user has requested a reload of a page with
  // blocked ads (e.g., via an infobar).
  void OnReloadRequested();

  static void LogAction(SubresourceFilterAction action);

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

  // Returns nullptr if the frame is not activated (and therefore has no
  // subresource filter).
  AsyncDocumentSubresourceFilter* GetFrameFilter(
      content::RenderFrameHost* frame_host);

  // Returns the activation state of the frame's filter. If the frame is not
  // activated (and therefore has no subresource filter), returns base::nullopt.
  const base::Optional<subresource_filter::mojom::ActivationState>
  GetFrameActivationState(content::RenderFrameHost* frame_host);

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

  // Gets a filter for the navigation from |throttle|, creates and returns a new
  // filter, or returns |nullptr|. Also updates |frame_host_filter_map_| as
  // appropriate. |frame_host| is provided as |navigation_handle|'s getter
  // cannot be used when the navigation has not committed.
  // `did_inherit_opener_activation` will be set according to whether the
  // activation was inherited from the frame's same-origin opener.
  AsyncDocumentSubresourceFilter* FilterForFinishedNavigation(
      content::NavigationHandle* navigation_handle,
      ActivationStateComputingNavigationThrottle* throttle,
      content::RenderFrameHost* frame_host,
      bool& did_inherit_opener_activation);

  void RecordUmaHistogramsForMainFrameNavigation(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationLevel& activation_level,
      bool did_inherit_opener_activation);

  // For each RenderFrameHost where the last committed load has subresource
  // filtering activated, owns the corresponding AsyncDocumentSubresourceFilter.
  // A null filter indicates that the filter should be inherited from its
  // parent if the parent has one. This is possible if the last load was a
  // special navigation (see MaybeActivateSubframeSpecialUrls) or if no
  // navigations have committed.
  std::map<content::RenderFrameHost*,
           std::unique_ptr<AsyncDocumentSubresourceFilter>>
      frame_host_filter_map_;

  // Set of RenderFrameHosts that have had at least one committed or aborted
  // navigation.
  std::set<content::RenderFrameHost*> navigated_frames_;

  // For each ongoing navigation that requires activation state computation,
  // keeps track of the throttle that is carrying out that computation, so that
  // the result can be retrieved when the navigation is ready to commit. Keyed
  // by navigation id.
  std::map<int64_t, ActivationStateComputingNavigationThrottle*>
      ongoing_activation_throttles_;

  // Set of RenderFrameHosts that have been identified as ads. An RFH is an ad
  // subframe if any of the following conditions are met:
  // 1. Its navigation URL is in the filter list
  // 2. Its parent is a known ad subframe
  // 3. The RenderFrame declares the frame is an ad (see AdTracker in Blink)
  // 4. It's the result of moving an old ad subframe RFH to a new RFH (e.g.,
  //    OOPIF)
  std::set<const content::RenderFrameHost*> ad_frames_;

  // Map of RenderFrameHost's whose navigations have been identified as ads.
  // Contains information on the most current completed navigation for any given
  // RenderFrameHost. If a frame is not present in the map, it has not had a
  // navigation evaluated by the filter list.
  std::map<const content::RenderFrameHost*, LoadPolicy>
      navigation_load_policies_;

  content::WebContentsFrameReceiverSet<mojom::SubresourceFilterHost> receiver_;

  base::ScopedObservation<SubresourceFilterObserverManager,
                          SubresourceFilterObserver>
      scoped_observation_{this};

  // Lazily instantiated in EnsureRulesetHandle when the first page level
  // activation is triggered. Will go away when there are no more activated
  // RenderFrameHosts (i.e. activated_frame_hosts_ is empty).
  std::unique_ptr<VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<PageLoadStatistics> statistics_;

  // True if the current committed main frame load in this WebContents has
  // notified the delegate that a subresource was disallowed. The callback
  // should only be called at most once per main frame load.
  bool current_committed_load_has_notified_disallowed_load_ = false;

  // This member outlives this class.
  VerifiedRulesetDealer::Handle* dealer_handle_;

  std::unique_ptr<SubresourceFilterClient> client_;

  base::WeakPtrFactory<ContentSubresourceFilterThrottleManager>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterThrottleManager);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_
