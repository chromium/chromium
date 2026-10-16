// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/dependency_graph.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

// Escapes |id| to be a valid ID in the DOT format [1]. This is implemented as
// enclosing the string in quotation marks, and escaping any quotation marks
// found with backslashes.
// [1] http://www.graphviz.org/content/dot-language
std::string Escape(std::string_view id) {
  std::string result = "\"";
  result.reserve(id.size() + 2);  // +2 for the enclosing quotes.
  size_t after_last_quot = 0;
  size_t next_quot = id.find('"');
  while (next_quot != std::string_view::npos) {
    result.append(id.substr(after_last_quot, next_quot - after_last_quot));
    result.append("\"");
    after_last_quot = next_quot + 1;
    next_quot = id.find('"', after_last_quot);
  }
  result.append(id.substr(after_last_quot, id.size() - after_last_quot));
  result.append("\"");
  return result;
}

}  // namespace

DependencyGraph::DependencyGraph() = default;

DependencyGraph::~DependencyGraph() = default;

void DependencyGraph::AddNode(DependencyNode* node) {
  all_nodes_.push_back(node);
  construction_order_.clear();
}

void DependencyGraph::RemoveNode(DependencyNode* node) {
  std::erase(all_nodes_, node);

  std::erase_if(edges_, [node](const auto& edge) {
    return edge.first == node || edge.second == node;
  });

  construction_order_.clear();
}

void DependencyGraph::AddEdge(DependencyNode* depended,
                              DependencyNode* dependee) {
  edges_.insert(std::make_pair(depended, dependee));
  construction_order_.clear();
}

bool DependencyGraph::GetConstructionOrder(
    std::vector<raw_ptr<DependencyNode, VectorExperimental>>* order) {
  if (construction_order_.empty() && !BuildConstructionOrder())
    return false;

  *order = construction_order_;
  return true;
}

bool DependencyGraph::GetDestructionOrder(
    std::vector<raw_ptr<DependencyNode, VectorExperimental>>* order) {
  if (construction_order_.empty() && !BuildConstructionOrder())
    return false;

  // Destroy nodes in reverse order.
  *order = {construction_order_.rbegin(), construction_order_.rend()};
  return true;
}

bool DependencyGraph::BuildConstructionOrder() {
  // Step 1: Build a set of nodes with no incoming edges.
  absl::flat_hash_map<DependencyNode*, size_t> incoming_edge_count;
  incoming_edge_count.reserve(all_nodes_.size());
  for (const auto& pair : edges_) {
    ++incoming_edge_count[pair.second];
  }

  base::circular_deque<DependencyNode*> queue;
  for (DependencyNode* node : all_nodes_) {
    if (!incoming_edge_count.contains(node)) {
      queue.push_back(node);
    }
  }

  // Step 2: Do the Kahn topological sort.
  std::vector<raw_ptr<DependencyNode, VectorExperimental>> output;
  output.reserve(all_nodes_.size());
  size_t remaining_edges = edges_.size();
  while (!queue.empty()) {
    DependencyNode* node = queue.front();
    queue.pop_front();
    output.push_back(node);

    auto [first, last] = edges_.equal_range(node);
    for (auto it = first; it != last; ++it) {
      DependencyNode* dest = it->second;
      --remaining_edges;
      if (--incoming_edge_count[dest] == 0) {
        queue.push_back(dest);
      }
    }
  }

  if (remaining_edges != 0) {
    // Dependency graph has a cycle.
    return false;
  }

  construction_order_ = std::move(output);
  return true;
}

std::string DependencyGraph::DumpAsGraphviz(
    const std::string& toplevel_name,
    const base::RepeatingCallback<std::string(DependencyNode*)>&
        node_name_callback) const {
  std::string result("digraph {\n");
  std::string escaped_toplevel_name = Escape(toplevel_name);

  // Make a copy of all nodes.
  base::circular_deque<DependencyNode*> nodes(std::from_range, all_nodes_);

  // State all dependencies and remove |second| so we don't generate an
  // implicit dependency on the top level node.
  result.append("  /* Dependencies */\n");
  for (const auto& pair : edges_) {
    result.append("  ");
    result.append(Escape(node_name_callback.Run(pair.second)));
    result.append(" -> ");
    result.append(Escape(node_name_callback.Run(pair.first)));
    result.append(";\n");

    base::Erase(nodes, pair.second);
  }

  // Every node that doesn't depend on anything else will implicitly depend on
  // the top level node.
  result.append("\n  /* Toplevel attachments */\n");
  for (DependencyNode* node : nodes) {
    result.append("  ");
    result.append(Escape(node_name_callback.Run(node)));
    result.append(" -> ");
    result.append(escaped_toplevel_name);
    result.append(";\n");
  }

  result.append("\n  /* Toplevel node */\n");
  result.append("  ");
  result.append(escaped_toplevel_name);
  result.append(" [shape=box];\n");

  result.append("}\n");
  return result;
}
