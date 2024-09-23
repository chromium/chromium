// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_

#include <cstdint>
#include <memory>
#include <unordered_set>

#include "base/dcheck_is_on.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/node_set_view.h"

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace performance_manager {

class GraphOwned;
class GraphRegistered;
class FrameNode;
class FrameNodeObserver;
class InitializingFrameNodeObserver;
class NodeDataDescriberRegistry;
class PageNode;
class PageNodeObserver;
class ProcessNode;
class ProcessNodeObserver;
class SystemNode;
class SystemNodeObserver;
class WorkerNode;
class WorkerNodeObserver;

template <typename DerivedType>
class GraphRegisteredImpl;

// Represents a graph of the nodes representing a single browser. Maintains a
// set of nodes that can be retrieved in different ways, some indexed. Keeps
// a list of observers that are notified of node addition and removal.
class Graph {
 public:
  using NodeSet = std::unordered_set<const Node*>;
  template <class NodeViewPtr>
  using NodeSetView = NodeSetView<NodeSet, NodeViewPtr>;

  Graph();

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  virtual ~Graph();

  // Adds an |observer| on the graph. It is safe for observers to stay
  // registered on the graph at the time of its death.
  virtual void AddFrameNodeObserver(FrameNodeObserver* observer) = 0;
  virtual void AddPageNodeObserver(PageNodeObserver* observer) = 0;
  virtual void AddProcessNodeObserver(ProcessNodeObserver* observer) = 0;
  virtual void AddSystemNodeObserver(SystemNodeObserver* observer) = 0;
  virtual void AddWorkerNodeObserver(WorkerNodeObserver* observer) = 0;

  // Removes an |observer| from the graph.
  virtual void RemoveFrameNodeObserver(FrameNodeObserver* observer) = 0;
  virtual void RemovePageNodeObserver(PageNodeObserver* observer) = 0;
  virtual void RemoveProcessNodeObserver(ProcessNodeObserver* observer) = 0;
  virtual void RemoveSystemNodeObserver(SystemNodeObserver* observer) = 0;
  virtual void RemoveWorkerNodeObserver(WorkerNodeObserver* observer) = 0;

  // For convenience, allows you to pass ownership of an object to the graph.
  // Useful for attaching observers that will live with the graph until it dies.
  // If you can name the object you can also take it back via "TakeFromGraph".
  virtual void PassToGraphImpl(std::unique_ptr<GraphOwned> graph_owned) = 0;
  virtual std::unique_ptr<GraphOwned> TakeFromGraph(
      GraphOwned* graph_owned) = 0;

  // Templated PassToGraph helper that also returns a pointer to the object,
  // which makes it easy to use PassToGraph in constructors.
  template <typename DerivedType>
  DerivedType* PassToGraph(std::unique_ptr<DerivedType> graph_owned) {
    DerivedType* object = graph_owned.get();
    PassToGraphImpl(std::move(graph_owned));
    return object;
  }

  // A TakeFromGraph helper for taking back the ownership of a GraphOwned
  // subclass.
  template <typename DerivedType>
  std::unique_ptr<DerivedType> TakeFromGraphAs(DerivedType* graph_owned) {
    return base::WrapUnique(
        static_cast<DerivedType*>(TakeFromGraph(graph_owned).release()));
  }

  // Registers an object with this graph. It is expected that no more than one
  // object of a given type is registered at a given moment, and that all
  // registered objects are unregistered before graph tear-down.
  virtual void RegisterObject(GraphRegistered* object) = 0;

  // Unregisters the provided |object|, which must previously have been
  // registered with "RegisterObject". It is expected that all registered
  // objects are unregistered before graph tear-down.
  virtual void UnregisterObject(GraphRegistered* object) = 0;

  // Returns the registered object of the given type, nullptr if none has been
  // registered.
  template <typename DerivedType>
  DerivedType* GetRegisteredObjectAs() {
    // Be sure to access the TypeId provided by GraphRegisteredImpl, in case
    // this class has other TypeId implementations.
    GraphRegistered* object =
        GetRegisteredObject(GraphRegisteredImpl<DerivedType>::TypeId());
    return static_cast<DerivedType*>(object);
  }

  // Returns the single system node.
  virtual const SystemNode* GetSystemNode() const = 0;

  // Returns a collection of all known nodes of the given type.
  virtual NodeSetView<const ProcessNode*> GetAllProcessNodes() const = 0;
  virtual NodeSetView<const FrameNode*> GetAllFrameNodes() const = 0;
  virtual NodeSetView<const PageNode*> GetAllPageNodes() const = 0;
  virtual NodeSetView<const WorkerNode*> GetAllWorkerNodes() const = 0;

  // Returns true if the graph only contains the default nodes.
  virtual bool HasOnlySystemNode() const = 0;

  // Returns the associated UKM recorder if it is defined.
  virtual ukm::UkmRecorder* GetUkmRecorder() const = 0;

  // Returns the data describer registry.
  virtual NodeDataDescriberRegistry* GetNodeDataDescriberRegistry() const = 0;

  // The following functions are implementation detail and should not need to be
  // used by external clients. They provide the ability to safely downcast to
  // the underlying implementation.
  virtual uintptr_t GetImplType() const = 0;
  virtual const void* GetImpl() const = 0;

  // Allows code that is not explicitly aware of the Graph sequence to determine
  // if they are in fact on the right sequence. Prefer to use the
  // DCHECK_ON_GRAPH_SEQUENCE macro.
#if DCHECK_IS_ON()
  virtual bool IsOnGraphSequence() const = 0;
#endif

  // Adds/removes a special type of FrameNodeObserver that needs to initialize
  // a property on frame nodes before other observers are notified of their
  // existence. This should be used sparingly.
  virtual void AddInitializingFrameNodeObserver(
      InitializingFrameNodeObserver* frame_node_observer) = 0;
  virtual void RemoveInitializingFrameNodeObserver(
      InitializingFrameNodeObserver* frame_node_observer) = 0;

 private:
  // Retrieves the object with the given |type_id|, returning nullptr if none
  // exists. Clients must use the GetRegisteredObjectAs wrapper instead.
  virtual GraphRegistered* GetRegisteredObject(uintptr_t type_id) = 0;
};

#if DCHECK_IS_ON()
#define DCHECK_ON_GRAPH_SEQUENCE(graph) DCHECK(graph->IsOnGraphSequence())
#else
// Compiles to a nop, and will eat ostream input.
#define DCHECK_ON_GRAPH_SEQUENCE(graph) DCHECK(true)
#endif

// Helper class for passing ownership of objects to a graph.
class GraphOwned {
 public:
  GraphOwned();

  GraphOwned(const GraphOwned&) = delete;
  GraphOwned& operator=(const GraphOwned&) = delete;

  virtual ~GraphOwned();

  // Called when the object is passed into the graph.
  virtual void OnPassedToGraph(Graph* graph) = 0;

  // Called when the object is removed from the graph, either via an explicit
  // call to Graph::TakeFromGraph, or prior to the Graph being destroyed.
  virtual void OnTakenFromGraph(Graph* graph) = 0;

  // Returns a pointer to the owning Graph. The will return nullptr before
  // OnPassedToGraph() and after OnTakenFromGraph(), and a valid pointer at all
  // other times.
  Graph* GetOwningGraph() const;

 private:
  // GraphImpl is allowed to call PassToGraphImpl and TakeFromGraphImpl.
  friend class GraphImpl;

  // GraphOwnedAndRegistered overrides PassToGraphImpl and TakeFromGraphImpl.
  template <typename SelfType>
  friend class GraphOwnedAndRegistered;

  // Only friends can override these. The default implementations just call
  // OnPassedToGraph() and OnTakenFromGraph(). Helper classes like
  // GraphOwnedAndRegistered can override these to add actions, while their
  // subclasses continue to override OnPassedToGraph and OnTakenFromGraph
  // without having to remember to call the inherited methods.
  virtual void PassToGraphImpl(Graph* graph);
  virtual void TakeFromGraphImpl(Graph* graph);

  SEQUENCE_CHECKER(sequence_checker_);

  // Pointer back to the owning graph.
  raw_ptr<Graph> graph_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
};

// A default implementation of GraphOwned.
class GraphOwnedDefaultImpl : public GraphOwned {
 public:
  GraphOwnedDefaultImpl();

  GraphOwnedDefaultImpl(const GraphOwnedDefaultImpl&) = delete;
  GraphOwnedDefaultImpl& operator=(const GraphOwnedDefaultImpl&) = delete;

  ~GraphOwnedDefaultImpl() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override {}
  void OnTakenFromGraph(Graph* graph) override {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_
