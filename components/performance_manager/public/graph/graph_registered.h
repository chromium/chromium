// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_REGISTERED_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_REGISTERED_H_

#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

// This provides functionality that allows an instance of a graph-associated
// object to be looked up by type, allowing the graph to act as a rendezvous
// point. It enforces singleton semantics, so there may be no more than one
// instance of an object of a given type registered with the same graph at the
// same time. All registration and unregistration must happen on the PM
// sequence. It is illegal to register more than one object of the same class at
// a time, and all registered objects must be unregistered prior to graph tear-
// down. This is intended to be used as follows:
//
// class Foo : public GraphRegisteredImpl<Foo> {
//   ...
// };
//
// void InvokedOnGraph(Graph* graph) {
//   Foo* foo = ... some instance of Foo ...
//   DCHECK(graph->RegisterObject(foo));
// }
//
// void InvokedSometimeLaterOnGraph(Graph* graph) {
//   Foo* foo = graph->GetRegisteredObjectAs<Foo>();
//   foo->DoSomething();
// }
//
// This may easily (and commonly) be combined with GraphOwned, allowing the
// registered object to be owned by the graph as well. The
// GraphOwnedAndRegistered helper will register the object automatically when
// PassToGraph is called. For example:
//
// class Bar : public GraphOwnedAndRegistered<Bar> {
//   void OnPassedToGraph(Graph* graph) override {
//     // RegisterObject was called before OnPassedToGraph.
//     graph->AddFrameNodeObserver(this);
//   }
//   void OnTakenFromGraph(Graph* graph) override {
//     graph->RemoveFrameNodeObserver(this);
//     // Will call UnregisterObject after OnTakenFromGraph.
//   }
// };
//
// void RunDuringInitialization() {
//   PerformanceManager::PassToGraph(std::make_unique<Bar>());
// }
//
// void InvokedSometimeLaterOnGraph(Graph* graph, bool done) {
//   Bar* bar = graph->GetRegisteredObjectAs<Bar>();
//   bar->DoSomething();
//   if (done) {
//     graph->TakeFromGraph(bar);
//     CHECK(!graph->GetRegisteredObjectAs<Bar>());
//   }
// }

template <typename SelfType>
class GraphRegisteredImpl;

// Interface that graph registered objects must implement. Should only be
// implemented via GraphRegisteredImpl or GraphOwnedAndRegistered.
class GraphRegistered {
 public:
  GraphRegistered(const GraphRegistered&) = delete;
  GraphRegistered& operator=(const GraphRegistered&) = delete;
  virtual ~GraphRegistered();

  // Returns the unique type of the object.
  virtual uintptr_t GetTypeId() const = 0;

 protected:
  template <typename SelfType>
  friend class GraphRegisteredImpl;

  GraphRegistered();
};

// Fully implements GraphRegistered. Clients should derive from this class.
template <typename SelfType>
class GraphRegisteredImpl : public GraphRegistered {
 public:
  GraphRegisteredImpl() = default;
  ~GraphRegisteredImpl() override = default;

  // The static TypeId associated with this class.
  static uintptr_t TypeId() {
    // The pointer to this object acts as a unique key that identifies the type
    // at runtime. Note that the address of this should be taken only from a
    // single library, as a different address will be returned from each library
    // into which a given data type is linked. Note that if base/type_id ever
    // becomes a thing, this should use that!
    static constexpr int kTypeId = 0;
    return reinterpret_cast<uintptr_t>(&kTypeId);
  }

  // GraphRegistered implementation:
  uintptr_t GetTypeId() const override { return TypeId(); }

  // Helper function for looking up the registered object of this type from the
  // provided graph. Syntactic sugar for "Graph::GetRegisteredObjectAs".
  static SelfType* GetFromGraph(Graph* graph) {
    return graph->GetRegisteredObjectAs<SelfType>();
  }

  // Returns true if this object is the registered object in the graph, false
  // otherwise. Useful for DCHECKing contract conditions.
  bool IsRegistered(Graph* graph) const { return GetFromGraph(graph) == this; }

  // Returns true if no object of this type is registered in the graph, false
  // otherwise. Useful for DCHECKing contract conditions.
  static bool NothingRegistered(Graph* graph) {
    return GetFromGraph(graph) == nullptr;
  }
};

// Helper for classes that are both GraphOwned and GraphRegistered.
template <typename SelfType>
class GraphOwnedAndRegistered : public GraphRegisteredImpl<SelfType>,
                                public GraphOwned {
 public:
  GraphOwnedAndRegistered() = default;
  ~GraphOwnedAndRegistered() override = default;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override {}
  void OnTakenFromGraph(Graph* graph) override {}

 private:
  void PassToGraphImpl(Graph* graph) override {
    graph->RegisterObject(this);
    GraphOwned::PassToGraphImpl(graph);
  }

  void TakeFromGraphImpl(Graph* graph) override {
    GraphOwned::TakeFromGraphImpl(graph);
    graph->UnregisterObject(this);
  }
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_REGISTERED_H_