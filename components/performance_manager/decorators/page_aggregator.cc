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

  if (frame_node->IsActive()) {
    // Decrement the form interaction, user edits and freezing origin trial
    // opt-out counters for this page if needed.
    if (frame_node->HadFormInteraction()) {
      data.UpdateActiveFrameCountForFormInteraction(
          /*is_active_with_form_interaction=*/false);
    }
    if (frame_node->HadUserEdits()) {
      data.UpdateActiveFrameCountForUserEdits(
          /*is_active_with_user_edits=*/false);
    }
    if (frame_node->HasFreezingOriginTrialOptOut()) {
      data.UpdateActiveFrameCountForFreezingOriginTrialOptOut(
          /*is_active_with_freezing_origin_trial_opt_out=*/false);
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

void PageAggregator::OnIsActiveChanged(const FrameNode* frame_node) {
  auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
  Data& data = GetOrCreateData(page_node);

  const bool is_active = frame_node->IsActive();
  if (frame_node->HadFormInteraction()) {
    data.UpdateActiveFrameCountForFormInteraction(is_active);
  }
  if (frame_node->HadUserEdits()) {
    data.UpdateActiveFrameCountForUserEdits(is_active);
  }
  if (frame_node->HasFreezingOriginTrialOptOut()) {
    data.UpdateActiveFrameCountForFreezingOriginTrialOptOut(is_active);
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
  if (frame_node->IsActive()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateActiveFrameCountForFormInteraction(
        frame_node->HadFormInteraction());
  }
}

void PageAggregator::OnHadUserEditsChanged(const FrameNode* frame_node) {
  if (frame_node->IsActive()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateActiveFrameCountForUserEdits(frame_node->HadUserEdits());
  }
}

void PageAggregator::OnFrameHasFreezingOriginTrialOptOutChanged(
    const FrameNode* frame_node) {
  if (frame_node->IsActive()) {
    auto* page_node = PageNodeImpl::FromNode(frame_node->GetPageNode());
    Data& data = GetOrCreateData(page_node);
    data.UpdateActiveFrameCountForFreezingOriginTrialOptOut(
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

base::DictValue PageAggregator::DescribePageNodeData(
    const PageNode* node) const {
  auto* page_node_impl = PageNodeImpl::FromNode(node);
  if (!Data::Exists(page_node_impl)) {
    return base::DictValue();
  }
  Data& data = Data::Get(page_node_impl);
  return data.Describe();
}

}  // namespace performance_manager
