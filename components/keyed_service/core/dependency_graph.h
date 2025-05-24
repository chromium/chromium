// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_GRAPH_H_
#define COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_GRAPH_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service_export.h"

class DependencyNode;

// Dynamic graph of dependencies between nodes.
class KEYED_SERVICE_EXPORT DependencyGraph {
 public:
  DependencyGraph();

  DependencyGraph(const DependencyGraph&) = delete;
  DependencyGraph& operator=(const DependencyGraph&) = delete;

  ~DependencyGraph();

  // Adds/Removes a node from our list of live nodes. Removing will
  // also remove live dependency links.
  void AddNode(DependencyNode* node);
  void RemoveNode(DependencyNode* node);

  // Adds a dependency between two nodes.
  void AddEdge(DependencyNode* depended, DependencyNode* dependee);

  // Topologically sorts nodes to produce a safe construction order
  // (all nodes after their dependees).
  [[nodiscard]] bool GetConstructionOrder(
      std::vector<raw_ptr<DependencyNode, VectorExperimental>>* order);

  // Topologically sorts nodes to produce a safe destruction order
  // (all nodes before their dependees).
  [[nodiscard]] bool GetDestructionOrder(
      std::vector<raw_ptr<DependencyNode, VectorExperimental>>* order);

  // Returns representation of the dependency graph in graphviz format.
  std::string DumpAsGraphviz(
      const std::string& toplevel_name,
      const base::RepeatingCallback<std::string(DependencyNode*)>&
          node_name_callback) const;

 private:
  typedef std::multimap<DependencyNode*,
                        raw_ptr<DependencyNode, CtnExperimental>>
      EdgeMap;

  // Populates |construction_order_| with computed construction order.
  // Returns true on success.
  [[nodiscard]] bool BuildConstructionOrder();

  // Keeps track of all live nodes (see AddNode, RemoveNode).
  std::vector<raw_ptr<DependencyNode, VectorExperimental>> all_nodes_;

  // Keeps track of edges of the dependency graph.
  EdgeMap edges_;

  // Cached construction order (needs rebuild with BuildConstructionOrder
  // when empty).
  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_GRAPH_H_
