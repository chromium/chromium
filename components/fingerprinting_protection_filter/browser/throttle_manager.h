// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_THROTTLE_MANAGER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_THROTTLE_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/content/shared/browser/page_load_statistics.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

class GURL;

namespace content {
class NavigationHandle;
class NavigationThrottle;
class Page;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;
class ActivationStateComputingNavigationThrottle;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

class FingerprintingProtectionWebContentsHelper;

// The `ThrottleManager` manages throttles that calculate frame activation
// states and child frame navigation filtering, within a given Page.
//
// This class is created for each `Page` that is a "root". Most `Pages` are
// roots so that each of (e.g. primary page, prerender, BFCache'd page, etc.)
// uses its own separate and independent `DocumentSubresourceFilter` (e.g. each
// computes main frame activation separately). Fenced frames are an exception. A
// fenced frame does create a separate `Page` but is considered a "child" of its
// embedder; behaviorally, we treat it like a regular iframe. See
// `IsInSubresourceFilterRoot` in subresource_filter_utils.cc. The term "main
// frame" is avoided in this code to avoid ambiguity; instead, the main frame of
// a page that is a root is called a "root frame" while other frames are called
// "child frames".
//
// Since this class is associated with a `Page`, cross-document navigation to a
// new `Page` will create a new instance of this class.
//
// Instances of this class are created by the
// `FingerprintingProtectionWebContentsHelper` class, of which there is 1 per
// `WebContents`, on navigation starts that will create a new eligible `Page`.
// This class is initially owned by the `NavigationHandle` that creates it. If
// the navigation commits, this class will be transferred to be owned by the\
// `Page` it is associated with. Otherwise it will be destroyed with the
// `NavigationHandle`.
class ThrottleManager : public base::SupportsUserData::Data,
                        public mojom::FingerprintingProtectionHost {
 public:
  static const int kUserDataKey = 0;

  // Binds a remote in the given `RenderFrame` to the correct `ThrottleManager`
  // in the browser. If no manager is found, `pending_receiver` will not be
  // consumed and the agent's disconnect handler will be called if set.
  static void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::FingerprintingProtectionHost>
          pending_receiver,
      content::RenderFrameHost* render_frame_host);

  // Creates a ThrottleManager instance from the given parameters.
  // NOTE: Short-circuits out (returns nullptr) if the
  // `kEnableFingerprintingProtectionFilter` feature is not enabled.
  static std::unique_ptr<ThrottleManager> CreateForNewPage(
      subresource_filter::VerifiedRulesetDealer::Handle* dealer_handle,
      FingerprintingProtectionWebContentsHelper& web_contents_helper,
      content::NavigationHandle& initiating_navigation_handle,
      bool is_incognito);

  // Since the throttle manager is created for a page-creating navigation, then
  // transferred onto the page once created, it is accessible in both
  // navigation and post-navigation contexts. Once a throttle-manager-holding
  // page is created, the throttle manager will be transferred into its user
  // data. `FromPage` will retrieve the throttle manager from the given `Page`.
  // Note: a fenced frame page will not have a throttle manager so this will
  // return nullptr in that case.
  static ThrottleManager* FromPage(content::Page& page);

  // `FromNavigationHandle` will retrieve a throttle manager that should be used
  // for the given `navigation_handle`. This is a bit more subtle than
  // `FromPage` as only those navigations that create a throttle-manager-holding
  // page (i.e. currently all non-fenced frame pages) will store a throttle
  // manager, that is: main-frame, cross-document navigations that are not
  // making an existing page primary. In other cases, `FromNavigationHandle`
  // will look up the throttle manager from the page it is navigating in. This
  // cannot (will CHECK) be used for prerendering or BFCache activating
  // navigations because which page to get a throttle manager from is ambiguous:
  // the navigation occurs in the primary frame tree but the non-primary page is
  // the resulting page.
  static ThrottleManager* FromNavigationHandle(
      content::NavigationHandle& navigation_handle);

  ThrottleManager(
      subresource_filter::VerifiedRulesetDealer::Handle* dealer_handle,
      FingerprintingProtectionWebContentsHelper& web_contents_helper,
      content::NavigationHandle& initiating_navigation_handle,
      bool is_incognito);
  ~ThrottleManager() override;

  // Disallow copy and assign.
  ThrottleManager(const ThrottleManager&) = delete;
  ThrottleManager& operator=(const ThrottleManager&) = delete;

  // This method inspects `navigation_handle` and attaches navigation throttles
  // appropriately, based on the current state of frame activation.
  //
  // 1. Child frame navigation filtering throttles are appended if the parent
  // frame is activated.
  // 2. Activation state computing throttles are appended if either the
  // navigation is a root frame navigation, or if the parent frame is activated.
  //
  // Note that there are currently no constraints on the ordering of throttles.
  void MaybeAppendNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles);

  subresource_filter::VerifiedRuleset::Handle* ruleset_handle_for_testing() {
    return ruleset_handle_.get();
  }

  void NotifyDisallowLoadPolicy(content::NavigationHandle* navigation_handle);

 protected:
  FRIEND_TEST_ALL_PREFIXES(
      ThrottleManagerEnabledTest,
      ThrottleManagerLifetime_DidFinishInFrameNavigationSucceeds);

  // These look like WebContentsObserver overrides but they are not, they're
  // called explicitly from the WebContentsHelper, which is a
  // WebContentsObserver, but only for the appropriate throttle manager.

  // "InFrame" here means that the navigation doesn't move a page between frame
  // trees. i.e. it is not a prerender activation.
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

  // Similar to above, called from the WebContentsHelper.
  void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::mojom::ActivationState& activation_state,
      const subresource_filter::ActivationDecision& activation_decision);

 private:
  friend FingerprintingProtectionWebContentsHelper;

  void LogActivationDecisionUkm(content::NavigationHandle* navigation_handle);

  // Keeps track of a filter that is associated with a document that already
  // has its activation computed.
  class FilterHandle : public content::DocumentUserData<FilterHandle> {
   public:
    ~FilterHandle() override;

    subresource_filter::AsyncDocumentSubresourceFilter* filter() {
      return filter_.get();
    }

   private:
    // No public constructors to force going through static methods of
    // DocumentUserData (e.g. CreateForCurrentDocument).
    explicit FilterHandle(
        content::RenderFrameHost* rfh,
        std::unique_ptr<subresource_filter::AsyncDocumentSubresourceFilter>
            filter);

    friend content::DocumentUserData<FilterHandle>;

    std::unique_ptr<subresource_filter::AsyncDocumentSubresourceFilter> filter_;

    DOCUMENT_USER_DATA_KEY_DECL();
  };

  // Keeps track of child activation throttles that are still waiting for
  // the activation computation for a navigation to complete.
  class ChildActivationThrottleHandle
      : public content::NavigationHandleUserData<
            ChildActivationThrottleHandle> {
   public:
    ~ChildActivationThrottleHandle() override;

    subresource_filter::ActivationStateComputingNavigationThrottle* throttle() {
      return throttle_;
    }

   private:
    // No public constructors to force going through static methods of
    // NavigationHandleUserData (e.g. CreateForNavigationHandle).
    explicit ChildActivationThrottleHandle(
        content::NavigationHandle& navigation_handle,
        subresource_filter::ActivationStateComputingNavigationThrottle*
            throttle);

    friend content::NavigationHandleUserData<ChildActivationThrottleHandle>;

    // Owned by the Chrome browser client.
    raw_ptr<subresource_filter::ActivationStateComputingNavigationThrottle>
        throttle_;

    NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
  };

  // Will return nullptr if the parent frame of this navigation is not
  // activated (and therefore has no filter).
  subresource_filter::AsyncDocumentSubresourceFilter* GetParentFrameFilter(
      content::NavigationHandle* child_frame_navigation);

  // Returns nullptr if the frame is not activated (and therefore has no
  // filter).
  subresource_filter::AsyncDocumentSubresourceFilter* GetFrameFilter(
      content::RenderFrameHost* frame_host);

  // Returns the activation state of the frame's filter. If the frame is not
  // activated (and therefore has no filter), returns std::nullopt.
  const std::optional<subresource_filter::mojom::ActivationState>
  GetFrameActivationState(content::RenderFrameHost* frame_host);

  // Calls NotifyOnBlockedResources() on `web_contents_helper_` at most once per
  // committed, non-same-page navigation in the main frame. `frame_host`
  // specifies the frame that blocked the subresource.
  void MaybeNotifyOnBlockedResource(content::RenderFrameHost* frame_host);

  subresource_filter::mojom::ActivationState
  ActivationStateForNextCommittedLoad(
      content::NavigationHandle* navigation_handle);

  // mojom::FingerprintingProtectionHost:
  void DidDisallowFirstSubresource() override;
  void CheckActivation(CheckActivationCallback callback) override;

  void SetDocumentLoadStatistics(
      subresource_filter::mojom::DocumentLoadStatisticsPtr statistics) override;

  // Gets a filter for the navigation from `throttle`, creates and returns a new
  // filter, or returns `nullptr`. Also adds `FilterHandle` objects as
  // DocumentUserData as appropriate. `frame_host` is provided as
  // `navigation_handle`'s getter cannot be used when the navigation has not
  // committed. `did_inherit_opener_activation` will be set according to whether
  // the activation was inherited from the frame's same-origin opener.
  subresource_filter::AsyncDocumentSubresourceFilter*
  FilterForFinishedNavigation(
      content::NavigationHandle* navigation_handle,
      subresource_filter::ActivationStateComputingNavigationThrottle* throttle,
      content::RenderFrameHost* frame_host,
      bool& did_inherit_opener_activation);

  void RecordUmaHistogramsForRootNavigation(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::mojom::ActivationLevel& activation_level,
      bool did_inherit_opener_activation);

  // Receiver set for all RenderFrames in this throttle manager's page.
  content::RenderFrameHostReceiverSet<mojom::FingerprintingProtectionHost>
      receivers_;

  // Lazily instantiated in EnsureRulesetHandle when the first page level
  // activation is triggered. Will go away when there are no more activated
  // RenderFrameHosts (i.e. activated_frame_hosts_ is empty).
  std::unique_ptr<subresource_filter::VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<subresource_filter::PageLoadStatistics> statistics_;

  // TODO(https://crbug.com/40280666): Add statistics once they are available in
  // a shared SubresourceFilter directory.

  // True if the current committed main frame load in this WebContents has
  // notified the delegate that a subresource was disallowed. The callback
  // should only be called at most once per main frame load.
  bool current_committed_load_has_notified_disallowed_load_ = false;

  // Unowned since the throttle manager should not outlive the Page that owns
  // it. The throttle manager is held as user data first on NavigationHandle,
  // then transferred to Page once it is created. Once the Page is created and
  // this class transferred onto it (in
  // FingerprintingProtectionWebContentsHelper) we'll set this member to point
  // to it.
  //
  // TODO(https://crbug.com/40280666): Triage dangling pointers.
  raw_ptr<content::Page, DanglingUntriaged> page_ = nullptr;

  // One ThrottleManger per page means one page activation decision per throttle
  // manager.
  subresource_filter::ActivationDecision page_activation_decision_ =
      subresource_filter::ActivationDecision::UNKNOWN;

  // The helper class is attached to the WebContents so it is guaranteed to
  // outlive this class which is owned by either a Page or NavigationHandle in
  // the WebContents.
  const raw_ref<FingerprintingProtectionWebContentsHelper> web_contents_helper_;

  // Whether or not the page-level activation computation is finished. The
  // activation state won't be returned to the renderer until this is true.
  bool page_level_activation_computed_ = false;

  // Page-level `ActivationState`, stored to be queried by throttles on the
  // renderer.
  subresource_filter::mojom::ActivationState page_activation_state_;

  // Whether the  profile is in Incognito mode.
  bool is_incognito_;

  base::WeakPtrFactory<ThrottleManager> weak_ptr_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_THROTTLE_MANAGER_H_
