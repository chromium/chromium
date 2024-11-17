// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/subresource_filter/content/browser/safe_browsing_child_navigation_throttle.h"
#include "components/subresource_filter/content/mojom/subresource_filter.mojom.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class Page;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;
class ActivationStateComputingNavigationThrottle;
class ContentSubresourceFilterThrottleManager;
class ContentSubresourceFilterWebContentsHelper;
class PageLoadStatistics;
class ProfileInteractionManager;
class SubresourceFilterProfileContext;

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
// order to calculate frame activation states and child frame navigation
// filtering, within a given Page. It contains a mapping of all activated
// RenderFrameHosts, along with their associated DocumentSubresourceFilters.
//
// This class is created for each Page that is a "subresource filter root".
// Most Pages are subresource filter roots so that each of (e.g. primary page,
// prerender, BFCache'd page, etc.) uses its own separate and independent
// subresource filter (e.g. each computes main frame activation separately).
// Fenced frames are an exception. A fenced frame does create a separate Page
// but is considered a "subresource filter child" of its embedder;
// behaviorally, the subresource filter treats it like a regular iframe. See
// IsInSubresourceFilterRoot in
// content_subresource_filter_web_contents_helper.cc. The term "main frame" is
// avoided in subresource filter code to avoid ambiguity; instead, the main
// frame of a page that is a subresource filter root is called a "root frame"
// while other frames are called "child frames".
//
// Since this class is associated with a Page, cross document navigation to a
// new Page will create a new instance of this class.
//
// Instances of this class are created by the
// ContentSubresourceFilterWebContentsHelper class, of which there is 1 per
// WebContents, on navigation starts that will create a new eligible Page. This
// class is initially owned by the NavigationHandle that creates it. If the
// navigation commits, this class will be transferred to be owned by the Page
// it is associated with. Otherwise it will be destroyed with the
// NavigationHandle.
//
// TODO(bokan): The lifetime management and observer pattern seems like it will
// be common to all features that want to observe events and track state on a
// per Page basis. The ContentSubresourceFilterWebContentsHelper pattern or
// something like it should be wrapped up into a common and reusable //content
// API. See:
// https://docs.google.com/document/d/1p-IXk8hI5ucWRf5vJEi9K_YvJXsTr8kbvzGrjMcALDE/edit?usp=sharing
class ContentSubresourceFilterThrottleManager
    : public base::SupportsUserData::Data,
      public mojom::SubresourceFilterHost {
 public:
  static const int kUserDataKey = 0;

  // Binds a remote in the given RenderFrame to the correct
  // ContentSubresourceFilterThrottleManager in the browser.
  static void BindReceiver(mojo::PendingAssociatedReceiver<
                               mojom::SubresourceFilterHost> pending_receiver,
                           content::RenderFrameHost* render_frame_host);

  // Creates a ThrottleManager instance from the given parameters.
  // NOTE: Short-circuits out if the kSafeBrowsingSubresourceFilter feature is
  // not enabled.
  static std::unique_ptr<ContentSubresourceFilterThrottleManager>
  CreateForNewPage(
      SubresourceFilterProfileContext* profile_context,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      VerifiedRulesetDealer::Handle* dealer_handle,
      ContentSubresourceFilterWebContentsHelper& web_contents_helper,
      content::NavigationHandle& initiating_navigation_handle);

  // Since the throttle manager is created for a page-creating navigation, then
  // transferred onto the page once created, it is accessible in both
  // navigation and post-navigation contexts. Once a throttle-manager-holding
  // page is created, the throttle manager will be transferred into its user
  // data. FromPage will retrieve the throttle manager from the given `page`.
  // Note: a fenced frame page will not have a throttle manager so this will
  // return nullptr in that case.
  static ContentSubresourceFilterThrottleManager* FromPage(content::Page& page);

  // FromNavigationHandle will retrieve a throttle manager that should be used
  // for the given `navigation_handle`. This is a bit more subtle than FromPage
  // as only those navigations that create a throttle-manager-holding page
  // (i.e. currently all non-fenced frame pages) will store a throttle manager,
  // that is: main-frame, cross-document navigations that are not making an
  // existing page primary. In other cases, FromNavigationHandle will look up
  // the throttle manager from the page it is navigating in. This cannot (will
  // CHECK) be used for prerendering or BFCache activating navigations because
  // which page to get a throttle manager from is ambiguous: the navigation
  // occurs in the primary frame tree but the non-primary page is the resulting
  // page.
  static ContentSubresourceFilterThrottleManager* FromNavigationHandle(
      content::NavigationHandle& navigation_handle);

  ContentSubresourceFilterThrottleManager(
      SubresourceFilterProfileContext* profile_context,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      VerifiedRulesetDealer::Handle* dealer_handle,
      ContentSubresourceFilterWebContentsHelper& web_contents_helper,
      content::NavigationHandle& initiating_navigation_handle);
  ~ContentSubresourceFilterThrottleManager() override;

  // Disallow copy and assign.
  ContentSubresourceFilterThrottleManager(
      const ContentSubresourceFilterThrottleManager&) = delete;
  ContentSubresourceFilterThrottleManager& operator=(
      const ContentSubresourceFilterThrottleManager&) = delete;

  // This method inspects `navigation_handle` and attaches navigation throttles
  // appropriately, based on the current state of frame activation.
  //
  // 1. Child frame navigation filtering throttles are appended if the parent
  // frame is activated.
  // 2. Activation state computing throttles are appended if either the
  // navigation is a subresource filter root frame navigation, or if the parent
  // frame is activated.
  //
  // Note that there is currently no constraints on the ordering of throttles.
  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles);

  PageLoadStatistics* page_load_statistics() const { return statistics_.get(); }

  ProfileInteractionManager* profile_interaction_manager_for_testing() {
    return profile_interaction_manager_.get();
  }

  VerifiedRuleset::Handle* ruleset_handle_for_testing() {
    return ruleset_handle_.get();
  }

  // Returns whether the identified frame is considered to be an ad.
  bool IsFrameTaggedAsAd(content::FrameTreeNodeId frame_tree_node_id) const;
  // Returns whether `frame_host` is in a frame considered to be an ad.
  bool IsRenderFrameHostTaggedAsAd(content::RenderFrameHost* frame_host) const;

  // Returns whether the last navigation resource in the given frame was
  // detected to be an ad. A null optional indicates there was no previous
  // navigation or the last navigation was not evaluated by the subresource
  // filter. Load policy is determined by presence of the navigation url in the
  // filter list.
  std::optional<LoadPolicy> LoadPolicyForLastCommittedNavigation(
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Called when the user has requested a reload of a page with
  // blocked ads (e.g., via an infobar).
  void OnReloadRequested();

  // Invoked when an ads violation is detected in `rfh`.
  void OnAdsViolationTriggered(content::RenderFrameHost* rfh,
                               mojom::AdsViolation triggered_violation);

  static void LogAction(SubresourceFilterAction action);

  void SetIsAdFrameForTesting(content::RenderFrameHost* render_frame_host,
                              bool is_ad_frame);

  // Returns the matching FrameAdEvidence for the frame indicated by
  // `render_frame_host` or `std::nullopt` if there is none (i.e. the frame is
  // a main frame, or no navigation or commit has yet occurred and no evidence
  // has been reported by the renderer).
  std::optional<blink::FrameAdEvidence> GetAdEvidenceForFrame(
      content::RenderFrameHost* render_frame_host);

 protected:
  // These look like WebContentsObserver overrides but they are not, they're
  // called explicitly from the WebContentsHelper, which is a
  // WebContentsObserver, but only for the appropriate throttle manager.
  void RenderFrameDeleted(content::RenderFrameHost* frame_host);
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id);
  // "InFrame" here means that the navigation doesn't move a page between frame
  // trees. i.e.  it is not a prerender activation.
  void ReadyToCommitInFrameNavigation(
      content::NavigationHandle* navigation_handle);
  void DidFinishInFrameNavigation(content::NavigationHandle* navigation_handle,
                                  bool is_initial_navigation);
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url);
  void DidBecomePrimaryPage();

  // Called when this manager is moved from a NavigationHandle to a Page. In
  // most cases this will be the Page of the RenderFrameHost the main-frame
  // navigation is committing into. However, for non-committing, initial
  // navigations this can be the initial RFH's Page. This is only called once
  // by the page that is associated with the throttle manager (e.g. a fenced
  // frame Page doesn't own a throttle manager)
  void OnPageCreated(content::Page& page);

  // Similar to above, these are called from the WebContentsHelper which is a
  // SubresourceFilterObserver.
  void OnPageActivationComputed(content::NavigationHandle* navigation_handle,
                                const mojom::ActivationState& activation_state);
  void OnChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy);

 private:
  friend ContentSubresourceFilterWebContentsHelper;

  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           SubframeNavigationTaggedAsAdByRenderer);
  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           GrandchildNavigationTaggedAsAdByRenderer);
  FRIEND_TEST_ALL_PREFIXES(
      SitePerProcessContentSubresourceFilterThrottleManagerTest,
      AdTagCarriesAcrossProcesses);
  FRIEND_TEST_ALL_PREFIXES(ContentSubresourceFilterThrottleManagerTest,
                           FirstDisallowedLoadCalledOutOfOrder);
  std::unique_ptr<SafeBrowsingChildNavigationThrottle>
  MaybeCreateChildNavigationThrottle(
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
  // activated (and therefore has no subresource filter), returns std::nullopt.
  const std::optional<subresource_filter::mojom::ActivationState>
  GetFrameActivationState(content::RenderFrameHost* frame_host);

  // Calls MaybeShowNotification on `profile_interaction_manager_` at most once
  // per committed, non-same-page navigation in the main frame. `frame_host`
  // specifies the frame that blocked the subresource.
  void MaybeShowNotification(content::RenderFrameHost* frame_host);

  VerifiedRuleset::Handle* EnsureRulesetHandle();
  void DestroyRulesetHandleIfNoLongerUsed();

  // Prefer the NavigationHandle version where possible as there are better
  // guard-rails for deriving the correct frame in edge cases.
  blink::FrameAdEvidence& EnsureFrameAdEvidence(
      content::NavigationHandle* navigation_handle);
  blink::FrameAdEvidence& EnsureFrameAdEvidence(
      content::RenderFrameHost* render_frame_host);
  blink::FrameAdEvidence& EnsureFrameAdEvidence(
      content::FrameTreeNodeId frame_tree_node_id,
      content::FrameTreeNodeId parent_frame_tree_node_id);

  mojom::ActivationState ActivationStateForNextCommittedLoad(
      content::NavigationHandle* navigation_handle);

  // Registers `render_frame_host` as an ad frame. If the frame later moves to
  // a new process its RenderHost will be told that it's an ad.
  void OnFrameIsAd(content::RenderFrameHost* render_frame_host);

  // Registers `frame_host` as a frame that was created by ad script.
  void OnChildFrameWasCreatedByAdScript(content::RenderFrameHost* frame_host);

  // mojom::SubresourceFilterHost:
  void DidDisallowFirstSubresource() override;
  void FrameIsAd() override;
  void FrameWasCreatedByAdScript() override;
  void AdScriptDidCreateFencedFrame(
      const blink::RemoteFrameToken& placeholder_token) override;
  void SetDocumentLoadStatistics(
      mojom::DocumentLoadStatisticsPtr statistics) override;
  void OnAdsViolationTriggered(mojom::AdsViolation violation) override;

  // Gets a filter for the navigation from `throttle`, creates and returns a new
  // filter, or returns `nullptr`. Also updates `frame_host_filter_map_` as
  // appropriate. `frame_host` is provided as `navigation_handle`'s getter
  // cannot be used when the navigation has not committed.
  // `did_inherit_opener_activation` will be set according to whether the
  // activation was inherited from the frame's same-origin opener.
  AsyncDocumentSubresourceFilter* FilterForFinishedNavigation(
      content::NavigationHandle* navigation_handle,
      ActivationStateComputingNavigationThrottle* throttle,
      content::RenderFrameHost* frame_host,
      bool& did_inherit_opener_activation);

  void RecordUmaHistogramsForRootNavigation(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationLevel& activation_level,
      bool did_inherit_opener_activation);

  void RecordExperimentalUmaHistogramsForNavigation(
      content::NavigationHandle* navigation_handle,
      bool passed_through_ready_to_commit);

  // Sets whether the frame is considered an ad frame. If the value has changed,
  // we also update the replication state and inform observers.
  void SetIsAdFrame(content::RenderFrameHost* render_frame_host,
                    bool is_ad_frame);

  // For each RenderFrameHost where the last committed load (or the initial load
  // if no committed load has occurred) has subresource filtering activated,
  // owns the corresponding AsyncDocumentSubresourceFilter.
  std::map<content::RenderFrameHost*,
           std::unique_ptr<AsyncDocumentSubresourceFilter>>
      frame_host_filter_map_;

  // For each ongoing navigation that requires activation state computation,
  // keeps track of the throttle that is carrying out that computation, so that
  // the result can be retrieved when the navigation is ready to commit. Keyed
  // by navigation id.
  std::map<int64_t,
           raw_ptr<ActivationStateComputingNavigationThrottle, CtnExperimental>>
      ongoing_activation_throttles_;

  // The set of navigations that have passed through ReadyToCommitNavigation,
  // but haven't yet passed through DidFinishNavigation. Keyed by navigation id.
  base::flat_set<int64_t> ready_to_commit_navigations_;

  // Set of frames that have been identified as ads, identified by FrameTreeNode
  // ID. A RenderFrameHost is an ad frame iff the FrameAdEvidence
  // corresponding to the frame indicates that it is.
  base::flat_set<content::FrameTreeNodeId> ad_frames_;

  // Map of child frames, keyed by FrameTreeNode ID, with value being the
  // evidence for or against the frames being ads. This evidence is updated
  // whenever a navigation's LoadPolicy is calculated.
  std::map<content::FrameTreeNodeId, blink::FrameAdEvidence>
      tracked_ad_evidence_;

  // Map of frames whose navigations have been identified as ads, keyed by
  // FrameTreeNode ID. Contains information on the most current completed
  // navigation for any given frames. If a frame is not present in the map, it
  // has not had a navigation evaluated by the filter list.
  std::map<content::FrameTreeNodeId, LoadPolicy> navigation_load_policies_;

  // Receiver set for all RenderFrames in this throttle manager's page.
  content::RenderFrameHostReceiverSet<mojom::SubresourceFilterHost> receiver_;

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
  raw_ptr<VerifiedRulesetDealer::Handle, AcrossTasksDanglingUntriaged>
      dealer_handle_;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

  std::unique_ptr<ProfileInteractionManager> profile_interaction_manager_;

  // Unowned since the throttle manager cannot outlive the Page that owns it.
  // The throttle manager is held as user data first on NavigationHandle, then
  // transferred to Page once it is created. Once the Page is created and this
  // class transferred onto it (in ContentSubresourceFilterWebContentsHelper)
  // we'll set this member to point to it.
  raw_ptr<content::Page> page_ = nullptr;

  // The helper class is attached to the WebContents so it is guaranteed to
  // outlive this class which is owned by either a Page or NavigationHandle in
  // the WebContents.
  const raw_ref<ContentSubresourceFilterWebContentsHelper> web_contents_helper_;

  base::WeakPtrFactory<ContentSubresourceFilterThrottleManager>
      weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_THROTTLE_MANAGER_H_
