// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_feature_observer_client.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

namespace {

void OnChangeNodeUsing(content::GlobalRenderFrameHostId id,
                       blink::mojom::ObservedFeatureType feature_type,
                       bool is_using,
                       GraphImpl* graph) {
  FrameNodeImpl* frame_node = graph->GetFrameNodeById(
      RenderProcessHostId(id.child_id), id.frame_routing_id);
  if (!frame_node)
    return;

  switch (feature_type) {
    case blink::mojom::ObservedFeatureType::kWebLock:
      frame_node->SetIsHoldingWebLock(is_using);
      return;

    // TODO(crbug.com/40634530): Rename
    // FrameNodeImpl::SetIsHoldingIndexedDBLock() to
    // SetIsHoldingIndexedDBConnections().
    case blink::mojom::ObservedFeatureType::kIndexedDBConnection:
      frame_node->SetIsHoldingIndexedDBLock(is_using);
      return;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace

PerformanceManagerFeatureObserverClient::
    PerformanceManagerFeatureObserverClient() = default;

PerformanceManagerFeatureObserverClient::
    ~PerformanceManagerFeatureObserverClient() = default;

void PerformanceManagerFeatureObserverClient::OnStartUsing(
    content::GlobalRenderFrameHostId id,
    blink::mojom::ObservedFeatureType feature_type) {
  bool is_using = true;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&OnChangeNodeUsing, id, feature_type, is_using));
}

void PerformanceManagerFeatureObserverClient::OnStopUsing(
    content::GlobalRenderFrameHostId id,
    blink::mojom::ObservedFeatureType feature_type) {
  bool is_using = false;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&OnChangeNodeUsing, id, feature_type, is_using));
}

}  // namespace performance_manager
