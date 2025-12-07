// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_TEST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_TEST_UTILS_H_

#include <map>
#include <optional>
#include <set>
#include <utility>

#include "base/scoped_observation.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_filter {

// This class can be used to observe subresource filtering events associated
// with a particular web contents. Particular events can be expected by using
// the Get* methods.
class TestSubresourceFilterObserver : public SubresourceFilterObserver,
                                      public content::WebContentsObserver {
 public:
  explicit TestSubresourceFilterObserver(content::WebContents* web_contents);

  TestSubresourceFilterObserver(const TestSubresourceFilterObserver&) = delete;
  TestSubresourceFilterObserver& operator=(
      const TestSubresourceFilterObserver&) = delete;

  ~TestSubresourceFilterObserver() override;

  // SubresourceFilterObserver:
  void OnSubresourceFilterGoingAway() override;
  void OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const mojom::ActivationState& activation_state) override;
  void OnChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      LoadPolicy load_policy) override;
  void OnIsAdFrameChanged(content::RenderFrameHost* render_frame_host,
                          bool is_ad_frame) override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::optional<mojom::ActivationLevel> GetPageActivation(
      const GURL& url) const;
  std::optional<LoadPolicy> GetChildFrameLoadPolicy(const GURL& url) const;

  bool GetIsAdFrame(content::FrameTreeNodeId frame_tree_node_id) const;

  std::optional<mojom::ActivationLevel> GetPageActivationForLastCommittedLoad()
      const;

  using SafeBrowsingCheck =
      std::pair<safe_browsing::SBThreatType, safe_browsing::ThreatMetadata>;
  std::optional<SafeBrowsingCheck> GetSafeBrowsingResult(const GURL& url) const;

 private:
  std::map<GURL, LoadPolicy> child_frame_load_evaluations_;

  // Set of FrameTreeNode IDs representing frames tagged as ads.
  std::set<content::FrameTreeNodeId> ad_frames_;

  std::map<GURL, mojom::ActivationLevel> page_activations_;
  std::map<GURL, SafeBrowsingCheck> safe_browsing_checks_;
  std::map<content::NavigationHandle*, mojom::ActivationLevel>
      pending_activations_;
  std::optional<mojom::ActivationLevel> last_committed_activation_;

  base::ScopedObservation<SubresourceFilterObserverManager,
                          SubresourceFilterObserver>
      scoped_observation_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_OBSERVER_TEST_UTILS_H_
