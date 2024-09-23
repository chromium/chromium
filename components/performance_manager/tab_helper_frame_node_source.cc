// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/tab_helper_frame_node_source.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

TabHelperFrameNodeSource::TabHelperFrameNodeSource()
    : performance_manager_tab_helper_observations_(this) {}

TabHelperFrameNodeSource::~TabHelperFrameNodeSource() {
  DCHECK(observed_frame_nodes_.empty());
  DCHECK(!performance_manager_tab_helper_observations_.IsObservingAnySource());
}

FrameNodeImpl* TabHelperFrameNodeSource::GetFrameNode(
    content::GlobalRenderFrameHostId render_process_host_id) {
  // Retrieve the client's RenderFrameHost and its associated
  // PerformanceManagerTabHelper.
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_host_id);
  if (!render_frame_host)
    return nullptr;

  PerformanceManagerTabHelper* performance_manager_tab_helper =
      PerformanceManagerTabHelper::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  if (!performance_manager_tab_helper)
    return nullptr;

  return performance_manager_tab_helper->GetFrameNode(render_frame_host);
}

void TabHelperFrameNodeSource::SubscribeToFrameNode(
    content::GlobalRenderFrameHostId render_process_host_id,
    OnbeforeFrameNodeRemovedCallback on_before_frame_node_removed_callback) {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_host_id);
  DCHECK(render_frame_host);

  PerformanceManagerTabHelper* performance_manager_tab_helper =
      PerformanceManagerTabHelper::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  DCHECK(performance_manager_tab_helper);

  FrameNodeImpl* frame_node =
      performance_manager_tab_helper->GetFrameNode(render_frame_host);

  // Add the frame to the set of observed frames that belongs to
  // |performance_manager_tab_helper|.
  if (AddObservedFrameNode(performance_manager_tab_helper, frame_node)) {
    // Start observing the tab helper only if this is the first observed frame
    // that is associated with it.
    performance_manager_tab_helper_observations_.AddObservation(
        performance_manager_tab_helper);
  }

  // Then remember the frame's callback.
  bool inserted =
      frame_node_callbacks_
          .insert(std::make_pair(
              frame_node, std::move(on_before_frame_node_removed_callback)))
          .second;
  DCHECK(inserted);
}

void TabHelperFrameNodeSource::UnsubscribeFromFrameNode(
    content::GlobalRenderFrameHostId render_process_host_id) {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_process_host_id);
  DCHECK(render_frame_host);

  PerformanceManagerTabHelper* performance_manager_tab_helper =
      PerformanceManagerTabHelper::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  DCHECK(performance_manager_tab_helper);

  FrameNodeImpl* frame_node =
      performance_manager_tab_helper->GetFrameNode(render_frame_host);

  // Remove the frame's callback without invoking it.
  size_t removed = frame_node_callbacks_.erase(frame_node);
  DCHECK_EQ(removed, 1u);

  // And also remove the frame from the set of observed frames that belongs to
  // |performance_manager_tab_helper|.
  if (RemoveObservedFrameNode(performance_manager_tab_helper, frame_node)) {
    // Stop observing that tab helper if there no longer are any observed
    // frames that are associated with it.
    performance_manager_tab_helper_observations_.RemoveObservation(
        performance_manager_tab_helper);
  }
}

void TabHelperFrameNodeSource::OnBeforeFrameNodeRemoved(
    PerformanceManagerTabHelper* performance_manager_tab_helper,
    FrameNodeImpl* frame_node) {
  // The tab helper owns many other frames than the ones this instance cares
  // about. Ignore irrelevant notifications.
  auto it = frame_node_callbacks_.find(frame_node);
  if (it == frame_node_callbacks_.end())
    return;

  // Invoke the frame's callback and remove it.
  std::move(it->second).Run(frame_node);
  frame_node_callbacks_.erase(it);

  // And also remove the frame from the set of observed frames that belong to
  // |performance_manager_tab_helper|.
  if (RemoveObservedFrameNode(performance_manager_tab_helper, frame_node)) {
    // Stop observing that tab helper if there no longer are any observed
    // frames that are associated with it.
    performance_manager_tab_helper_observations_.RemoveObservation(
        performance_manager_tab_helper);
  }
}

bool TabHelperFrameNodeSource::AddObservedFrameNode(
    PerformanceManagerTabHelper* performance_manager_tab_helper,
    FrameNodeImpl* frame_node) {
  auto insertion_result =
      observed_frame_nodes_.insert({performance_manager_tab_helper, {}});

  auto& frame_nodes = insertion_result.first->second;
  bool inserted = frame_nodes.insert(frame_node).second;
  DCHECK(inserted);

  return insertion_result.second;
}

bool TabHelperFrameNodeSource::RemoveObservedFrameNode(
    PerformanceManagerTabHelper* performance_manager_tab_helper,
    FrameNodeImpl* frame_node) {
  auto it = observed_frame_nodes_.find(performance_manager_tab_helper);
  CHECK(it != observed_frame_nodes_.end(), base::NotFatalUntil::M130);

  base::flat_set<raw_ptr<FrameNodeImpl, CtnExperimental>>& frame_nodes =
      it->second;
  size_t removed = frame_nodes.erase(frame_node);
  DCHECK_EQ(removed, 1u);

  if (frame_nodes.empty()) {
    observed_frame_nodes_.erase(it);
    return true;
  }

  return false;
}

}  // namespace performance_manager
