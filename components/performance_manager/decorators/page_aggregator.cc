// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/page_aggregator.h"

#include <cstdint>

#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "PageAggregator";

PageAggregatorData& GetOrCreateData(PageNodeImpl* page_node) {
  if (!PageAggregatorData::Exists(page_node)) {
    return PageAggregatorData::Create(page_node);
  }
  return PageAggregatorData::Get(page_node);
}

const PageNode* GetPageNodeFromEither(const FrameNode* frame_node1,
                                      const FrameNode* frame_node2) {
  // Sanity check.
  CHECK(frame_node1 || frame_node2);
  if (frame_node1 && frame_node2) {
    CHECK_EQ(frame_node1->GetPageNode(), frame_node2->GetPageNode());
  }

  if (frame_node1) {
    return frame_node1->GetPageNode();
  } else {
    return frame_node2->GetPageNode();
  }
}

}  // namespace

PageAggregator::PageAggregator() = default;
PageAggregator::~PageAggregator() = default;

void PageAggregator::OnFrameNodeAdded(const FrameNode* frame_node) {
  CHECK(!frame_node->HadFormInteraction());
  CHECK(!frame_node->HadUserEdits());
  CHECK(!frame_node->IsHoldingWebLock());
  CHECK(!frame_node->IsHoldingIndexedDBLock());
  CHECK(!frame_node->UsesWebRTC());
}

void PageAggregator::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());

  if (!Data::Exists(page_node)) {
    return;
  }

  Data& data = Data::Get(page_node);

  if (frame_node->IsCurrent()) {
    // Decrement the form interaction and user edits counters for this page if
    // needed.
    if (frame_node->HadFormInteraction()) {
      data.UpdateCurrentFrameCountForFormInteraction(false, page_node,
                                                     frame_node);
    }
    if (frame_node->HadUserEdits()) {
      data.UpdateCurrentFrameCountForUserEdits(false, page_node, frame_node);
    }
  }

  // It is not guaranteed that the graph will be notified that the frame has
  // released locks or stopped using WebRTC before it is notified of the frame
  // being deleted.
  if (frame_node->IsHoldingWebLock()) {
    data.UpdateFrameCountForWebLockUsage(false, page_node);
  }
  if (frame_node->IsHoldingIndexedDBLock()) {
    data.UpdateFrameCountForIndexedDBLockUsage(false, page_node);
  }
  if (frame_node->UsesWebRTC()) {
    data.UpdateFrameCountForWebRTCUsage(false, page_node);
  }
}

void PageAggregator::OnCurrentFrameChanged(
    const FrameNode* previous_frame_node,
    const FrameNode* current_frame_node) {
  auto* page_node = PageNodeImpl::FromNode(
      GetPageNodeFromEither(previous_frame_node, current_frame_node));
  Data& data = GetOrCreateData(page_node);

  // Check if either frame node had some form interaction or user edit, in this
  // case there's two possibilities:
  //   - The frame became current: The counter of current frames with form
  //     interactions should be increased.
  //   - The frame became non current: The counter of current frames with form
  //     interactions should be decreased.
  if (previous_frame_node) {
    const bool is_current = false;
    if (previous_frame_node->HadFormInteraction()) {
      data.UpdateCurrentFrameCountForFormInteraction(is_current, page_node,
                                                     nullptr);
    }
    if (previous_frame_node->HadUserEdits()) {
      data.UpdateCurrentFrameCountForUserEdits(is_current, page_node, nullptr);
    }
  }
  if (current_frame_node) {
    const bool is_current = true;
    if (current_frame_node->HadFormInteraction()) {
      data.UpdateCurrentFrameCountForFormInteraction(is_current, page_node,
                                                     nullptr);
    }
    if (current_frame_node->HadUserEdits()) {
      data.UpdateCurrentFrameCountForUserEdits(is_current, page_node, nullptr);
    }
  }
}

void PageAggregator::OnFrameIsHoldingWebLockChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);
  data.UpdateFrameCountForWebLockUsage(frame_node->IsHoldingWebLock(),
                                       page_node);
}

void PageAggregator::OnFrameIsHoldingIndexedDBLockChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);
  data.UpdateFrameCountForIndexedDBLockUsage(
      frame_node->IsHoldingIndexedDBLock(), page_node);
}

void PageAggregator::OnFrameUsesWebRTCChanged(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);
  data.UpdateFrameCountForWebRTCUsage(frame_node->UsesWebRTC(), page_node);
}

void PageAggregator::OnHadFormInteractionChanged(const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateCurrentFrameCountForFormInteraction(
        frame_node->HadFormInteraction(), page_node, nullptr);
  }
}

void PageAggregator::OnHadUserEditsChanged(const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateCurrentFrameCountForUserEdits(frame_node->HadUserEdits(),
                                             page_node, nullptr);
  }
}

void PageAggregator::OnPassedToGraph(Graph* graph) {
  // This observer presumes that it's been added before any frame nodes exist in
  // the graph.
  DCHECK(graph->GetAllFrameNodes().empty());
  graph->AddFrameNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageAggregator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveFrameNodeObserver(this);
}

base::Value::Dict PageAggregator::DescribePageNodeData(
    const PageNode* node) const {
  auto* page_node_impl = PageNodeImpl::FromNode(node);
  if (!Data::Exists(page_node_impl)) {
    return base::Value::Dict();
  }
  Data& data = Data::Get(page_node_impl);
  return data.Describe();
}

}  // namespace performance_manager
