// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ATTRIBUTION_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ATTRIBUTION_HELPERS_H_

#include <set>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/function_ref.h"
#include "base/functional/overloaded.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace performance_manager {
class FrameNode;
class WorkerNode;
}  // namespace performance_manager

namespace resource_attribution {

// A set of nodes to include or exclude from a resource split.
using NodeSplitSet =
    std::set<absl::variant<const performance_manager::FrameNode*,
                           const performance_manager::WorkerNode*>>;

// Splits a resource of type T between all frames and workers hosted in
// `process_node`. `frame_setter` or `worker_setter` will be called for each
// node with that node's fraction of `resource_value`.
//
// Any node in `extra_nodes` will be included in the split as if it were hosted
// in `process_node`, and any node in `nodes_to_skip` will be excluded from the
// split.
template <typename T,
          // Template parameters are used to alias FrameNode and WorkerNode
          // without introducing them into the resource_attribution namespace.
          typename FrameNode = performance_manager::FrameNode,
          typename WorkerNode = performance_manager::WorkerNode,
          typename FrameSetter = base::FunctionRef<void(const FrameNode*, T)>,
          typename WorkerSetter = base::FunctionRef<void(const WorkerNode*, T)>>
void SplitResourceAmongFramesAndWorkers(
    T resource_value,
    const performance_manager::ProcessNode* process_node,
    const NodeSplitSet& extra_nodes,
    const NodeSplitSet& nodes_to_skip,
    FrameSetter frame_setter,
    WorkerSetter worker_setter);

// Splits a resource of type T between all frames and workers hosted in
// `process_node`. `frame_setter` or `worker_setter` will be called for each
// node with that node's fraction of `resource_value`.
template <typename T,
          // Template parameters are used to alias FrameNode and WorkerNode
          // without introducing them into the resource_attribution namespace.
          typename FrameNode = performance_manager::FrameNode,
          typename WorkerNode = performance_manager::WorkerNode,
          typename FrameSetter = base::FunctionRef<void(const FrameNode*, T)>,
          typename WorkerSetter = base::FunctionRef<void(const WorkerNode*, T)>>
void SplitResourceAmongFramesAndWorkers(
    T resource_value,
    const performance_manager::ProcessNode* process_node,
    FrameSetter frame_setter,
    WorkerSetter worker_setter) {
  SplitResourceAmongFramesAndWorkers(resource_value, process_node,
                                     /*extra_nodes=*/{},
                                     /*nodes_to_skip=*/{}, frame_setter,
                                     worker_setter);
}

// Implementation

template <typename T,
          typename FrameNode,
          typename WorkerNode,
          typename FrameSetter,
          typename WorkerSetter>
void SplitResourceAmongFramesAndWorkers(
    T resource_value,
    const performance_manager::ProcessNode* process_node,
    const NodeSplitSet& extra_nodes,
    const NodeSplitSet& nodes_to_skip,
    FrameSetter frame_setter,
    WorkerSetter worker_setter) {
  // Attribute the resources of the process to its frames and workers
  // Only renderers can host frames and workers.
  CHECK(process_node);
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }

  std::set<const FrameNode*> frame_nodes;
  for (const FrameNode* f : process_node->GetFrameNodes()) {
    if (!base::Contains(nodes_to_skip, f)) {
      frame_nodes.insert(f);
    }
  }

  std::set<const WorkerNode*> worker_nodes;
  for (const WorkerNode* w : process_node->GetWorkerNodes()) {
    if (!base::Contains(nodes_to_skip, w)) {
      worker_nodes.insert(w);
    }
  }

  for (const auto& frame_or_worker : extra_nodes) {
    CHECK(!base::Contains(nodes_to_skip, frame_or_worker));
    absl::visit(
        base::Overloaded{
            [&frame_nodes](const FrameNode* f) { frame_nodes.insert(f); },
            [&worker_nodes](const WorkerNode* w) { worker_nodes.insert(w); },
        },
        frame_or_worker);
  }

  const size_t frame_and_worker_node_count =
      frame_nodes.size() + worker_nodes.size();
  if (frame_and_worker_node_count == 0) {
    return;
  }

  // For now, equally split the process' resources among all of its frames and
  // workers.
  const T resource_estimate_part = resource_value / frame_and_worker_node_count;
  for (const FrameNode* frame : frame_nodes) {
    frame_setter(frame, resource_estimate_part);
  }
  for (const WorkerNode* worker : worker_nodes) {
    worker_setter(worker, resource_estimate_part);
  }
}

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_ATTRIBUTION_HELPERS_H_
