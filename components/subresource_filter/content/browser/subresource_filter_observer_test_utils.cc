// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"

#include "base/check.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TestSubresourceFilterObserver::TestSubresourceFilterObserver(
    content::WebContents* web_contents) {
  auto* manager =
      SubresourceFilterObserverManager::FromWebContents(web_contents);
  DCHECK(manager);
  scoped_observation_.Observe(manager);
  Observe(web_contents);
}

TestSubresourceFilterObserver::~TestSubresourceFilterObserver() {}

void TestSubresourceFilterObserver::OnSubresourceFilterGoingAway() {
  DCHECK(scoped_observation_.IsObserving());
  scoped_observation_.Reset();
}

void TestSubresourceFilterObserver::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const mojom::ActivationState& activation_state) {
  DCHECK(navigation_handle->IsInMainFrame());
  mojom::ActivationLevel level = activation_state.activation_level;
  page_activations_[navigation_handle->GetURL()] = level;
  pending_activations_[navigation_handle] = level;
}

void TestSubresourceFilterObserver::OnSubframeNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy) {
  subframe_load_evaluations_[navigation_handle->GetURL()] = load_policy;
}

void TestSubresourceFilterObserver::OnAdSubframeDetected(
    content::RenderFrameHost* render_frame_host,
    const FrameAdEvidence& ad_evidence) {
  ad_evidence_.emplace(render_frame_host->GetFrameTreeNodeId(), ad_evidence);
}

void TestSubresourceFilterObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  auto it = pending_activations_.find(navigation_handle);
  bool did_compute = it != pending_activations_.end();
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    if (did_compute)
      pending_activations_.erase(it);
    return;
  }

  if (did_compute) {
    last_committed_activation_ = it->second;
    pending_activations_.erase(it);
  } else {
    last_committed_activation_.reset();
  }
}

base::Optional<mojom::ActivationLevel>
TestSubresourceFilterObserver::GetPageActivation(const GURL& url) const {
  auto it = page_activations_.find(url);
  if (it != page_activations_.end())
    return it->second;
  return base::nullopt;
}

bool TestSubresourceFilterObserver::GetIsAdSubframe(
    int frame_tree_node_id) const {
  return base::Contains(ad_evidence_, frame_tree_node_id);
}

const FrameAdEvidence& TestSubresourceFilterObserver::GetEvidenceForAdSubframe(
    int frame_tree_node_id) const {
  auto it = ad_evidence_.find(frame_tree_node_id);
  DCHECK(it != ad_evidence_.end());
  return it->second;
}

base::Optional<LoadPolicy> TestSubresourceFilterObserver::GetSubframeLoadPolicy(
    const GURL& url) const {
  auto it = subframe_load_evaluations_.find(url);
  if (it != subframe_load_evaluations_.end())
    return it->second;
  return base::Optional<LoadPolicy>();
}

base::Optional<mojom::ActivationLevel>
TestSubresourceFilterObserver::GetPageActivationForLastCommittedLoad() const {
  return last_committed_activation_;
}

base::Optional<TestSubresourceFilterObserver::SafeBrowsingCheck>
TestSubresourceFilterObserver::GetSafeBrowsingResult(const GURL& url) const {
  auto it = safe_browsing_checks_.find(url);
  if (it != safe_browsing_checks_.end())
    return it->second;
  return base::Optional<SafeBrowsingCheck>();
}

void TestSubresourceFilterObserver::VerifyEvidenceForAdSubframe(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    FilterListEvidence filter_list_result,
    ScriptHeuristicEvidence created_by_ad_script) const {
  const FrameAdEvidence& ad_evidence =
      GetEvidenceForAdSubframe(frame_host->GetFrameTreeNodeId());
  EXPECT_TRUE(ad_evidence.is_complete());
  EXPECT_EQ(ad_evidence.parent_is_ad(), parent_is_ad);
  EXPECT_EQ(ad_evidence.filter_list_result(), filter_list_result);
  EXPECT_EQ(ad_evidence.created_by_ad_script(), created_by_ad_script);
}

}  // namespace subresource_filter
