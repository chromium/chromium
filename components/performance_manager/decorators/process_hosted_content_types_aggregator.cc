// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_hosted_content_types_aggregator.h"

#include "base/check.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

ProcessHostedContentTypesAggregator::ProcessHostedContentTypesAggregator() =
    default;

ProcessHostedContentTypesAggregator::~ProcessHostedContentTypesAggregator() =
    default;

void ProcessHostedContentTypesAggregator::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void ProcessHostedContentTypesAggregator::OnTakenFromGraph(Graph* graph) {
  graph->RemoveWorkerNodeObserver(this);
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
}

void ProcessHostedContentTypesAggregator::OnTypeChanged(
    const PageNode* page_node,
    PageType previous_type) {
  if (page_node->GetType() == PageType::kExtension) {
    // `PageType::kExtension` should be set early on the `PageNode`, before it
    // has the opportunity to create more than one main frame or any subframe.
    //
    // TODO(crbug.com/40194583): Change CHECKs to DCHECKs in September 2022 if
    // there are no crash report indicating that expectations are incorrect.
    CHECK_LE(page_node->GetMainFrameNodes().size(), 1U);
    if (auto* main_frame = page_node->GetMainFrameNode()) {
      CHECK(main_frame->GetChildFrameNodes().empty());
      FrameNodeImpl::FromNode(main_frame)
          ->process_node()
          ->add_hosted_content_type(ProcessNode::ContentType::kExtension);
    }
  }
}

void ProcessHostedContentTypesAggregator::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  // TODO(crbug.com/40194872): Decide if prerendered frames should be handled
  // differently.
  //
  // TODO(1241218, 1111084): A fenced frame should not be treated the same way
  // as a main frame.
  auto* frame_node_impl = FrameNodeImpl::FromNode(frame_node);
  auto* process_node_impl = frame_node_impl->process_node();
  process_node_impl->add_hosted_content_type(
      frame_node_impl->IsMainFrame() ? ProcessNode::ContentType::kMainFrame
                                     : ProcessNode::ContentType::kSubframe);

  if (frame_node_impl->page_node()->GetType() == PageType::kExtension) {
    process_node_impl->add_hosted_content_type(
        ProcessNode::ContentType::kExtension);
  }
}

void ProcessHostedContentTypesAggregator::OnIsAdFrameChanged(
    const FrameNode* frame_node) {
  auto* frame_node_impl = FrameNodeImpl::FromNode(frame_node);

  // No need to handle untagging as content hosted in the past is still counted.
  if (frame_node_impl->IsAdFrame()) {
    frame_node_impl->process_node()->add_hosted_content_type(
        ProcessNode::ContentType::kAd);
  }
}

void ProcessHostedContentTypesAggregator::OnURLChanged(
    const FrameNode* frame_node,
    const GURL& previous_value) {
  auto* frame_node_impl = FrameNodeImpl::FromNode(frame_node);
  frame_node_impl->process_node()->add_hosted_content_type(
      ProcessNode::ContentType::kNavigatedFrame);
}

void ProcessHostedContentTypesAggregator::OnWorkerNodeAdded(
    const WorkerNode* worker_node) {
  auto* worker_node_impl = WorkerNodeImpl::FromNode(worker_node);
  worker_node_impl->process_node()->add_hosted_content_type(
      ProcessNode::ContentType::kWorker);
}

}  // namespace performance_manager
