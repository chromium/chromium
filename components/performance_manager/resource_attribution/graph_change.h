// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_GRAPH_CHANGE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_GRAPH_CHANGE_H_

#include <optional>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/task/task_traits.h"
#include "url/origin.h"

namespace performance_manager {
class Node;
class ProcessNode;
}

namespace resource_attribution {

// Graph changes that can affect resource measurement distribution.
// These are all passed on the stack so don't need to use raw_ptr.
struct NoGraphChange {};

// Not technically a graph change, but modifies the distribution of FrameNode
// and WorkerNode measurements to OriginInBrowsingInstanceContexts the same way
// graph changes modify the distribution of measurements to PageContexts.
struct GraphChangeUpdateOrigin {
  GraphChangeUpdateOrigin(const performance_manager::Node* node,
                          std::optional<url::Origin> previous_origin);
  ~GraphChangeUpdateOrigin();

  GraphChangeUpdateOrigin(const GraphChangeUpdateOrigin&);
  GraphChangeUpdateOrigin& operator=(const GraphChangeUpdateOrigin&);
  GraphChangeUpdateOrigin(GraphChangeUpdateOrigin&&);
  GraphChangeUpdateOrigin& operator=(GraphChangeUpdateOrigin&&);

  raw_ptr<const performance_manager::Node> node;
  std::optional<url::Origin> previous_origin;
};

struct GraphChangeUpdateProcessPriority {
  GraphChangeUpdateProcessPriority(
      const performance_manager::ProcessNode* process_node,
      base::TaskPriority previous_priority)
      : process_node(process_node), previous_priority(previous_priority) {}

  raw_ptr<const performance_manager::ProcessNode> process_node;
  base::TaskPriority previous_priority;
};

using GraphChange = std::variant<NoGraphChange,
                                 GraphChangeUpdateOrigin,
                                 GraphChangeUpdateProcessPriority>;

}  // namespace resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RESOURCE_ATTRIBUTION_GRAPH_CHANGE_H_
