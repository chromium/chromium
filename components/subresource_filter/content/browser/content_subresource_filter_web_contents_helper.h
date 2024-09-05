// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_WEB_CONTENTS_HELPER_H_

#include <set>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}  // namespace safe_browsing

namespace subresource_filter {

class ContentSubresourceFilterThrottleManager;
class SubresourceFilterProfileContext;

// This class manages the lifetime and storage of
// ContentSubresourceFilterThrottleManager instances. This helper is attached
// to each WebContents and listens to navigations to ensure certain Page(s) in
// the WebContents have an associated throttle manager. A throttle manager is
// created for outermost pages. Fenced frames are treated as subframes and don't
// create a throttle manager; they use the throttle manager of their embedding
// page.
//
// This class also listens to events occurring in the WebContents and
// SubresourceFilter and, based on their context, routes the event to the
// throttle manager of the target page.
// TODO(bokan): This seems like a common pattern for a feature to want to
// observe events and track state on a per-Page basis. The WebContentsHelper
// pattern or something like it should be wrapped up into a common and reusable
// //content API. See:
// https://docs.google.com/document/d/1p-IXk8hI5ucWRf5vJEi9K_YvJXsTr8kbvzGrjMcALDE/edit?usp=sharing
class ContentSubresourceFilterWebContentsHelper
    : public content::WebContentsUserData<
          ContentSubresourceFilterWebContentsHelper>,
      public content::WebContentsObserver,
      public SubresourceFilterObserver {
 public:
  static void CreateForWebContents(
      content::WebContents* web_contents,
      SubresourceFilterProfileContext* profile_context,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      VerifiedRulesetDealer::Handle* dealer_handle);

  // Will get the helper from the given `page`'s WebContents.
  static ContentSubresourceFilterWebContentsHelper* FromPage(
      content::Page& page);

  explicit ContentSubresourceFilterWebContentsHelper(
      content::WebContents* web_contents,
      SubresourceFilterProfileContext* profile_context,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      VerifiedRulesetDealer::Handle* dealer_handle);
  ~ContentSubresourceFilterWebContentsHelper() override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Prefer to use the static methods on
  // ContentSubresourceFilterThrottleManager. See comments there.
  static ContentSubresourceFilterThrottleManager* GetThrottleManager(
      content::NavigationHandle& handle);
  static ContentSubresourceFilterThrottleManager* GetThrottleManager(
      content::Page& page);

  // Sets the SafeBrowsingDatabaseManager instance to use on new throttle
  // managers. Note, this will not update the database_manager_ value on
  // existing ContentSubresourceFilterThrottleManagers.
  void SetDatabaseManagerForTesting(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager);

  void WillDestroyThrottleManager(
      ContentSubresourceFilterThrottleManager* throttle_manager);

 protected:
  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* frame_host) override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
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
  void OnChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy) override;

 private:
  raw_ptr<SubresourceFilterProfileContext, DanglingUntriaged> profile_context_;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  raw_ptr<VerifiedRulesetDealer::Handle, DanglingUntriaged> dealer_handle_;

  // Set of frames across all pages in this WebContents that have had at least
  // one committed or aborted navigation. Keyed by FrameTreeNodeId.
  std::set<content::FrameTreeNodeId> navigated_frames_;

  base::ScopedObservation<SubresourceFilterObserverManager,
                          SubresourceFilterObserver>
      scoped_observation_{this};

  // Keep track of all active throttle managers. Unowned as a throttle manager
  // will notify this class when it's destroyed so we can remove it from this
  // set.
  base::flat_set<
      raw_ptr<ContentSubresourceFilterThrottleManager, CtnExperimental>>
      throttle_managers_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_SUBRESOURCE_FILTER_WEB_CONTENTS_HELPER_H_
