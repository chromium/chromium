// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace performance_manager {

class GraphObserver;
class GraphOwned;
class FrameNode;
class FrameNodeObserver;
class PageNode;
class PageNodeObserver;
class ProcessNode;
class ProcessNodeObserver;
class SystemNode;
class SystemNodeObserver;
class WorkerNode;
class WorkerNodeObserver;

// Represents a graph of the nodes representing a single browser. Maintains a
// set of nodes that can be retrieved in different ways, some indexed. Keeps
// a list of observers that are notified of node addition and removal.
class Graph {
 public:
  using Observer = GraphObserver;

  Graph();
  virtual ~Graph();

  // Adds an |observer| on the graph. It is safe for observers to stay
  // registered on the graph at the time of its death.
  virtual void AddGraphObserver(GraphObserver* observer) = 0;
  virtual void AddFrameNodeObserver(FrameNodeObserver* observer) = 0;
  virtual void AddPageNodeObserver(PageNodeObserver* observer) = 0;
  virtual void AddProcessNodeObserver(ProcessNodeObserver* observer) = 0;
  virtual void AddSystemNodeObserver(SystemNodeObserver* observer) = 0;
  virtual void AddWorkerNodeObserver(WorkerNodeObserver* observer) = 0;

  // Removes an |observer| from the graph.
  virtual void RemoveGraphObserver(GraphObserver* observer) = 0;
  virtual void RemoveFrameNodeObserver(FrameNodeObserver* observer) = 0;
  virtual void RemovePageNodeObserver(PageNodeObserver* observer) = 0;
  virtual void RemoveProcessNodeObserver(ProcessNodeObserver* observer) = 0;
  virtual void RemoveSystemNodeObserver(SystemNodeObserver* observer) = 0;
  virtual void RemoveWorkerNodeObserver(WorkerNodeObserver* observer) = 0;

  // For convenience, allows you to pass ownership of an object to the graph.
  // Useful for attaching observers that will live with the graph until it dies.
  // If you can name the object you can also take it back via "TakeFromGraph".
  virtual void PassToGraph(std::unique_ptr<GraphOwned> graph_owned) = 0;
  virtual std::unique_ptr<GraphOwned> TakeFromGraph(
      GraphOwned* graph_owned) = 0;

  // A TakeFromGraph helper for taking back the ownership of a GraphOwned
  // subclass.
  template <typename DerivedType>
  std::unique_ptr<DerivedType> TakeFromGraphAs(DerivedType* graph_owned) {
    return base::WrapUnique(
        static_cast<DerivedType*>(TakeFromGraph(graph_owned).release()));
  }

  // Returns a collection of all known nodes of the given type.
  virtual const SystemNode* FindOrCreateSystemNode() = 0;
  virtual std::vector<const ProcessNode*> GetAllProcessNodes() const = 0;
  virtual std::vector<const FrameNode*> GetAllFrameNodes() const = 0;
  virtual std::vector<const PageNode*> GetAllPageNodes() const = 0;
  virtual std::vector<const WorkerNode*> GetAllWorkerNodes() const = 0;

  // Returns true if the graph is currently empty.
  virtual bool IsEmpty() const = 0;

  // Returns the associated UKM recorder if it is defined.
  virtual ukm::UkmRecorder* GetUkmRecorder() const = 0;

  // The following functions are implementation detail and should not need to be
  // used by external clients. They provide the ability to safely downcast to
  // the underlying implementation.
  virtual uintptr_t GetImplType() const = 0;
  virtual const void* GetImpl() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Graph);
};

// Observer interface for the graph.
class GraphObserver {
 public:
  GraphObserver();
  virtual ~GraphObserver();

  // Called before the |graph| associated with this observer disappears. This
  // allows the observer to do any necessary cleanup work. Note that the
  // observer should remove itself from observing the graph using this
  // callback.
  // TODO(chrisha): Make this run before the destructor!
  // crbug.com/966840
  virtual void OnBeforeGraphDestroyed(Graph* graph) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

// Helper class for passing ownership of objects to a graph.
class GraphOwned {
 public:
  GraphOwned();
  virtual ~GraphOwned();

  // Called when the object is passed into the graph.
  virtual void OnPassedToGraph(Graph* graph) = 0;

  // Called when the object is removed from the graph, either via an explicit
  // call to Graph::TakeFromGraph, or prior to the Graph being destroyed.
  virtual void OnTakenFromGraph(Graph* graph) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphOwned);
};

// A default implementation of GraphOwned.
class GraphOwnedDefaultImpl : public GraphOwned {
 public:
  GraphOwnedDefaultImpl();
  ~GraphOwnedDefaultImpl() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override {}
  void OnTakenFromGraph(Graph* graph) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphOwnedDefaultImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_
