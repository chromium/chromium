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
}

PageAggregator::PageAggregator() = default;
PageAggregator::~PageAggregator() = default;

void PageAggregator::OnFrameNodeAdded(const FrameNode* frame_node) {
  CHECK(!frame_node->HadFormInteraction());
  CHECK(!frame_node->HadUserEdits());
  CHECK(!frame_node->IsHoldingWebLock());
  CHECK(!frame_node->IsHoldingIndexedDBLock());
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
  // released its lock before it is notified of the frame being deleted.
  if (frame_node->IsHoldingWebLock())
    data.UpdateFrameCountForWebLockUsage(false, page_node);
  if (frame_node->IsHoldingIndexedDBLock())
    data.UpdateFrameCountForIndexedDBLockUsage(false, page_node);
}

void PageAggregator::OnIsCurrentChanged(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);

  // Check if the frame node had some form interaction or user edit, in this
  // case there's two possibilities:
  //   - The frame became current: The counter of current frames with form
  //     interactions should be increased.
  //   - The frame became non current: The counter of current frames with form
  //     interactions should be decreased.
  if (frame_node->HadFormInteraction()) {
    data.UpdateCurrentFrameCountForFormInteraction(frame_node->IsCurrent(),
                                                   page_node, nullptr);
  }
  if (frame_node->HadUserEdits()) {
    data.UpdateCurrentFrameCountForUserEdits(frame_node->IsCurrent(), page_node,
                                             nullptr);
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
