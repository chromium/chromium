// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_hosted_content_types_aggregator.h"

#include "base/check.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

ProcessHostedContentTypesAggregator::ProcessHostedContentTypesAggregator() =
    default;

ProcessHostedContentTypesAggregator::~ProcessHostedContentTypesAggregator() =
    default;

void ProcessHostedContentTypesAggregator::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddFrameNodeObserver(this);
}

void ProcessHostedContentTypesAggregator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

void ProcessHostedContentTypesAggregator::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  auto* frame_node_impl = FrameNodeImpl::FromNode(frame_node);

  // TODO(1241909): Figure out how if prerendered frames should be handle
  //                differently.
  if (frame_node_impl->IsMainFrame()) {
    frame_node_impl->process_node()->add_hosted_content_type(
        ProcessNode::ContentType::kMainFrame);
  }
}

void ProcessHostedContentTypesAggregator::OnIsAdFrameChanged(
    const FrameNode* frame_node) {
  auto* frame_node_impl = FrameNodeImpl::FromNode(frame_node);

  // No need to handle untagging as content hosted in the past is still counted.
  if (frame_node_impl->is_ad_frame()) {
    frame_node_impl->process_node()->add_hosted_content_type(
        ProcessNode::ContentType::kAd);
  }
}

}  // namespace performance_manager
