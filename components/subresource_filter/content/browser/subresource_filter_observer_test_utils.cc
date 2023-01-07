// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
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

void TestSubresourceFilterObserver::OnChildFrameNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy) {
  child_frame_load_evaluations_[navigation_handle->GetURL()] = load_policy;
}

void TestSubresourceFilterObserver::OnIsAdFrameChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_frame) {
  if (is_ad_frame)
    ad_frames_.insert(render_frame_host->GetFrameTreeNodeId());
  else
    ad_frames_.erase(render_frame_host->GetFrameTreeNodeId());
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

absl::optional<mojom::ActivationLevel>
TestSubresourceFilterObserver::GetPageActivation(const GURL& url) const {
  auto it = page_activations_.find(url);
  if (it != page_activations_.end())
    return it->second;
  return absl::nullopt;
}

bool TestSubresourceFilterObserver::GetIsAdFrame(int frame_tree_node_id) const {
  return base::Contains(ad_frames_, frame_tree_node_id);
}

absl::optional<LoadPolicy>
TestSubresourceFilterObserver::GetChildFrameLoadPolicy(const GURL& url) const {
  auto it = child_frame_load_evaluations_.find(url);
  if (it != child_frame_load_evaluations_.end())
    return it->second;
  return absl::optional<LoadPolicy>();
}

absl::optional<mojom::ActivationLevel>
TestSubresourceFilterObserver::GetPageActivationForLastCommittedLoad() const {
  return last_committed_activation_;
}

absl::optional<TestSubresourceFilterObserver::SafeBrowsingCheck>
TestSubresourceFilterObserver::GetSafeBrowsingResult(const GURL& url) const {
  auto it = safe_browsing_checks_.find(url);
  if (it != safe_browsing_checks_.end())
    return it->second;
  return absl::optional<SafeBrowsingCheck>();
}

}  // namespace subresource_filter
