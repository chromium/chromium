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
  CHECK(!frame_node->IsHoldingBlockingIndexedDBLock());
  CHECK(!frame_node->UsesWebRTC());
}

void PageAggregator::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());

  if (!Data::Exists(page_node)) {
    return;
  }

  Data& data = Data::Get(page_node);

  if (frame_node->IsCurrent()) {
    // Decrement the form interaction, user edits and freezing origin trial
    // opt-out counters for this page if needed.
    if (frame_node->HadFormInteraction()) {
      data.UpdateCurrentFrameCountForFormInteraction(
          /*frame_had_form_interaction=*/false);
    }
    if (frame_node->HadUserEdits()) {
      data.UpdateCurrentFrameCountForUserEdits(/*frame_had_user_edits=*/false);
    }
    if (frame_node->HasFreezingOriginTrialOptOut()) {
      data.UpdateCurrentFrameCountForFreezingOriginTrialOptOut(false);
    }
  }

  // It is not guaranteed that the graph will be notified that the frame has
  // released locks or stopped using WebRTC before it is notified of the frame
  // being deleted.
  if (frame_node->IsHoldingWebLock()) {
    data.UpdateFrameCountForWebLockUsage(/*frame_is_holding_weblock=*/false);
  }
  if (frame_node->IsHoldingBlockingIndexedDBLock()) {
    data.UpdateFrameCountForBlockingIndexedDBLockUsage(
        /*frame_is_holding_blocking_indexeddb_lock=*/false);
  }
  if (frame_node->UsesWebRTC()) {
    data.UpdateFrameCountForWebRTCUsage(/*frame_uses_web_rtc=*/false);
  }
}

void PageAggregator::OnCurrentFrameChanged(
    const FrameNode* previous_frame_node,
    const FrameNode* current_frame_node) {
  auto* page_node = PageNodeImpl::FromNode(
      GetPageNodeFromEither(previous_frame_node, current_frame_node));
  Data& data = GetOrCreateData(page_node);

  // This lambda adjusts the form interaction, user edits and freezing origin
  // trial opt-out counters for a `frame_node` which just became current (if
  // `is_current` is true) or non-current (if `is_current` is false).
  auto adjust_counters = [&data](const FrameNode* frame_node, bool is_current) {
    if (frame_node->HadFormInteraction()) {
      data.UpdateCurrentFrameCountForFormInteraction(is_current);
    }
    if (frame_node->HadUserEdits()) {
      data.UpdateCurrentFrameCountForUserEdits(is_current);
    }
    if (frame_node->HasFreezingOriginTrialOptOut()) {
      data.UpdateCurrentFrameCountForFreezingOriginTrialOptOut(is_current);
    }
  };

  if (previous_frame_node) {
    adjust_counters(previous_frame_node, /*is_current=*/false);
  }

  if (current_frame_node) {
    adjust_counters(current_frame_node, /*is_current=*/true);
  }
}

void PageAggregator::OnFrameIsHoldingWebLockChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);
  data.UpdateFrameCountForWebLockUsage(frame_node->IsHoldingWebLock());
}

void PageAggregator::OnFrameIsHoldingBlockingIndexedDBLockChanged(
    const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);
  data.UpdateFrameCountForBlockingIndexedDBLockUsage(
      frame_node->IsHoldingBlockingIndexedDBLock());
}

void PageAggregator::OnFrameUsesWebRTCChanged(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);
  data.UpdateFrameCountForWebRTCUsage(frame_node->UsesWebRTC());
}

void PageAggregator::OnHadFormInteractionChanged(const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateCurrentFrameCountForFormInteraction(
        frame_node->HadFormInteraction());
  }
}

void PageAggregator::OnHadUserEditsChanged(const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateCurrentFrameCountForUserEdits(frame_node->HadUserEdits());
  }
}

void PageAggregator::OnFrameHasFreezingOriginTrialOptOutChanged(
    const FrameNode* frame_node) {
  if (frame_node->IsCurrent()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateCurrentFrameCountForFreezingOriginTrialOptOut(
        frame_node->HasFreezingOriginTrialOptOut());
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
