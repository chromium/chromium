// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/frozen_frame_aggregator.h"

#include "components/performance_manager/freezing/frozen_data.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

using LifecycleState = performance_manager::mojom::LifecycleState;

namespace {

const char kDescriberName[] = "FrozenFrameAggregator";

bool IsFrozen(const FrameNodeImpl* frame_node) {
  return frame_node->GetLifecycleState() == LifecycleState::kFrozen;
}

}  // namespace

FrozenFrameAggregator::FrozenFrameAggregator() = default;
FrozenFrameAggregator::~FrozenFrameAggregator() = default;

void FrozenFrameAggregator::OnFrameNodeAdded(const FrameNode* frame_node) {
  auto* frame_impl = FrameNodeImpl::FromNode(frame_node);
  DCHECK(!IsFrozen(frame_impl));  // A newly created node can never be frozen.
  AddOrRemoveFrame(frame_impl, 1);
}

void FrozenFrameAggregator::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  AddOrRemoveFrame(FrameNodeImpl::FromNode(frame_node), -1);
}

void FrozenFrameAggregator::OnCurrentFrameChanged(
    const FrameNode* previous_frame_node,
    const FrameNode* current_frame_node) {
  if (previous_frame_node) {
    auto* frame_impl = FrameNodeImpl::FromNode(previous_frame_node);
    CHECK(!frame_impl->IsCurrent());
    int32_t current_frame_delta = -1;
    int32_t frozen_frame_delta = IsFrozen(frame_impl) ? -1 : 0;
    UpdateFrameCounts(frame_impl, current_frame_delta, frozen_frame_delta);
  }
  if (current_frame_node) {
    auto* frame_impl = FrameNodeImpl::FromNode(current_frame_node);
    CHECK(frame_impl->IsCurrent());
    int32_t current_frame_delta = 1;
    int32_t frozen_frame_delta = IsFrozen(frame_impl) ? 1 : 0;
    UpdateFrameCounts(frame_impl, current_frame_delta, frozen_frame_delta);
  }
}

void FrozenFrameAggregator::OnFrameLifecycleStateChanged(
    const FrameNode* frame_node) {
  auto* frame_impl = FrameNodeImpl::FromNode(frame_node);
  if (!frame_impl->IsCurrent()) {
    return;
  }
  int32_t frozen_frame_delta = IsFrozen(frame_impl) ? 1 : -1;
  UpdateFrameCounts(frame_impl, 0, frozen_frame_delta);
}

void FrozenFrameAggregator::OnPassedToGraph(Graph* graph) {
  RegisterObservers(graph);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void FrozenFrameAggregator::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  UnregisterObservers(graph);
}

void FrozenFrameAggregator::OnPageNodeAdded(const PageNode* page_node) {
  DCHECK_EQ(LifecycleState::kRunning, page_node->GetLifecycleState());
  FrozenData::Create(PageNodeImpl::FromNode(page_node));
}

void FrozenFrameAggregator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  FrozenData::Create(ProcessNodeImpl::FromNode(process_node));
}

base::Value::Dict FrozenFrameAggregator::DescribePageNodeData(
    const PageNode* node) const {
  FrozenData& data = FrozenData::Get(PageNodeImpl::FromNode(node));
  return data.Describe();
}

base::Value::Dict FrozenFrameAggregator::DescribeProcessNodeData(
    const ProcessNode* node) const {
  FrozenData& data = FrozenData::Get(ProcessNodeImpl::FromNode(node));
  return data.Describe();
}

void FrozenFrameAggregator::RegisterObservers(Graph* graph) {
  // This observer presumes that it's been added before any nodes exist in the
  // graph.
  // TODO(chrisha): Add graph introspection functions to Graph.
  DCHECK(graph->HasOnlySystemNode());
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void FrozenFrameAggregator::UnregisterObservers(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

void FrozenFrameAggregator::AddOrRemoveFrame(FrameNodeImpl* frame_node,
                                             int32_t delta) {
  int32_t current_frame_delta = 0;
  int32_t frozen_frame_delta = 0;
  if (frame_node->IsCurrent()) {
    current_frame_delta = delta;
    if (IsFrozen(frame_node))
      frozen_frame_delta = delta;
  }

  UpdateFrameCounts(frame_node, current_frame_delta, frozen_frame_delta);
}

void FrozenFrameAggregator::UpdateFrameCounts(FrameNodeImpl* frame_node,
                                              int32_t current_frame_delta,
                                              int32_t frozen_frame_delta) {
  // If a non-current frame is added or removed the deltas can be zero. In this
  // case the logic can be aborted early to save some effort.
  if (current_frame_delta == 0 && frozen_frame_delta == 0)
    return;

  auto* page_node = frame_node->page_node();
  auto* process_node = frame_node->process_node();
  FrozenData& page_data = FrozenData::Get(page_node);
  FrozenData& process_data = FrozenData::Get(process_node);

  // We should only have frames attached to renderer processes.
  DCHECK_EQ(content::PROCESS_TYPE_RENDERER, process_node->GetProcessType());

  // Set the page lifecycle state based on the state of the frame tree.
  if (page_data.ChangeFrameCounts(current_frame_delta, frozen_frame_delta)) {
    page_node->SetLifecycleState(base::PassKey<FrozenFrameAggregator>(),
                                 page_data.AsLifecycleState());
  }

  // Update the process state, and notify when all frames in the tree are
  // frozen.
  if (process_data.ChangeFrameCounts(current_frame_delta, frozen_frame_delta) &&
      process_data.IsFrozen()) {
    process_node->OnAllFramesInProcessFrozen(
        base::PassKey<FrozenFrameAggregator>());
  }
}

}  // namespace performance_manager
