// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_INLINE_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_INLINE_DATA_H_

#include <utility>

#include "base/types/pass_key.h"
#include "components/performance_manager/graph/node_inline_data_impl.h"

namespace performance_manager {

// Helper classes for defining a user data class that is associated with nodes
// in the graph. At most one instance of each type of data may exist per node
// in the graph.
//
// An instance of node inline data has to be specifically permitted to be
// associated with individual node types, with this being enforced at compile
// time via the type system. A storage type must be specified which may be one
// of the following:
//
// - NodeInlineData<T>: Directly stored as a member in the node.
//
// - SparseNodeInlineData<T>: Stored in a unique_ptr<> as a member in the node.
//
// If the data is needed for most instances of a node type and for effectively
// the entire lifetime of the node, use NodeInlineData<T>.
//
// If the data is needed for most instances of a node type, but only for a
// relatively small portion of the node's lifetime, use
// SparseNodeInlineData<T>. The type will be stored as a member on the node, and
// as such it will be slightly faster to access but will always occupy its
// memory footprint, whether it is created or not.
//
// If the data is *not* needed for most instances of a node type, use
// NodeAttachedData<T> (See node_attached_data.h). The type will be stored in a
// unique_ptr, and will thus less memory is allocated when the type is not
// created.
//
// The functionality of this file is intended to be used as follows:
//
// First, add your data type to the node type (or types) that you want to
// associate your data with.
//
// For example:
//
// -- process_node_impl.h --
//
// class ProcessNodeImpl : (...)
//      (...)
//      public SupportsNodeInlineData<DataType1,
//                                    DataType2,
//                           ---->    YourDataType>    <----
//
// -- process_node_impl.h --
//
// Second, derive your data type from one of NodeInlineData<T> or
// SparseNodeInlineData<T>:
//
// -- your_data_type.h --
//
// class YourDataType : public SparseNodeInlineData<YourDataType> {
//  (...)
// };
//
// -- your_data_type.h --
//
// Once defined, your type instance can be created like so:
//
//   YourDataType& your_data_type = YourDataType::Create(node_impl, ..args..);
//
// If the instance already exists, you can access it using Get():
//
//   YourDataType& your_data_type = YourDataType::Get(node_impl);
//
// Optionally, you can preemptively destroy the instance once it is no longer
// needed:
//
//   YourDataType::Destroy(node_impl);

template <class T>
class NodeInlineData {
 public:
  using PassKey = base::PassKey<NodeInlineData>;

  // Returns true if the instance exists for this type.
  template <class NodeImplClass>
  static bool Exists(NodeImplClass* node);

  // Retrieves the instance associated with `node`. Asserts that it exists.
  // `Create()` must have been called before calling this.
  template <class NodeImplClass>
  static T& Get(NodeImplClass* node);

  // Retrieves the instance associated with `node`. Asserts that it exists.
  // `Create()` must have been called before calling this.
  template <class NodeImplClass>
  static const T& Get(const NodeImplClass* node);

  // Creates the instance.
  template <class NodeImplClass, class... Args>
  static T& Create(NodeImplClass* node, Args&&... args);

  // Destroys the instance associated with `node`. Asserts that it exists. Note
  // that calling this is optional. The instance will get cleaned up
  // automatically when the node is destroyed.
  template <class NodeImplClass>
  static void Destroy(NodeImplClass* node);
};

template <class T>
class SparseNodeInlineData : public NodeInlineData<T> {};

// Derived by node implementation classes to provide access to inline data.
// Private through the use of base::PassKey so that only the relevant
// NodeInlineData<T> can use it.
template <class... Ts>
class SupportsNodeInlineData {
 public:
  template <class T>
  bool NodeDataExists(base::PassKey<NodeInlineData<T>>);

  template <class T>
  T& GetNodeData(base::PassKey<NodeInlineData<T>>);

  template <class T>
  const T& GetNodeData(base::PassKey<NodeInlineData<T>>) const;

  template <class T, class... Args>
  T& CreateNodeData(base::PassKey<NodeInlineData<T>>, Args&&... args);

  template <class T>
  void DestroyNodeData(base::PassKey<NodeInlineData<T>>);

 protected:
  virtual ~SupportsNodeInlineData() = default;

  void DestroyNodeInlineDataStorage();

 private:
  template <class T>
  internal::Storage<T>& GetStorage();

  template <class T>
  const internal::Storage<T>& GetStorage() const;

  using UnderlyingStorage = std::tuple<internal::Storage<Ts>...>;
  UnderlyingStorage storage_;
};

///////////////////////////////////////////////////////////
// Everything below this point is implementation detail! //
///////////////////////////////////////////////////////////

// Implementation of NodeInlineData<T>:

// static
template <class T>
template <class NodeImplClass>
bool NodeInlineData<T>::Exists(NodeImplClass* node) {
  return node->NodeDataExists(PassKey());
}

// static
template <class T>
template <class NodeImplClass>
T& NodeInlineData<T>::Get(NodeImplClass* node) {
  return node->GetNodeData(PassKey());
}

// static
template <class T>
template <class NodeImplClass>
const T& NodeInlineData<T>::Get(const NodeImplClass* node) {
  return node->GetNodeData(PassKey());
}

// static
template <class T>
template <class NodeImplClass, class... Args>
T& NodeInlineData<T>::Create(NodeImplClass* node, Args&&... args) {
  return node->CreateNodeData(PassKey(), std::forward<Args>(args)...);
}

// static
template <class T>
template <class NodeImplClass>
void NodeInlineData<T>::Destroy(NodeImplClass* node) {
  node->DestroyNodeData(PassKey());
}

// Implementation of SupportsNodeInlineData<Ts...>:

template <class... Ts>
template <class T>
bool SupportsNodeInlineData<Ts...>::NodeDataExists(
    base::PassKey<NodeInlineData<T>>) {
  return GetStorage<T>().Exists();
}

template <class... Ts>
template <class T>
T& SupportsNodeInlineData<Ts...>::GetNodeData(
    base::PassKey<NodeInlineData<T>>) {
  return GetStorage<T>().Get();
}

template <class... Ts>
template <class T>
const T& SupportsNodeInlineData<Ts...>::GetNodeData(
    base::PassKey<NodeInlineData<T>>) const {
  return GetStorage<T>().Get();
}

template <class... Ts>
template <class T, class... Args>
T& SupportsNodeInlineData<Ts...>::CreateNodeData(
    base::PassKey<NodeInlineData<T>>,
    Args&&... args) {
  return GetStorage<T>().Create(std::forward<Args>(args)...);
}

template <class... Ts>
template <class T>
void SupportsNodeInlineData<Ts...>::DestroyNodeData(
    base::PassKey<NodeInlineData<T>>) {
  GetStorage<T>().Destroy();
}

template <class... Ts>
void SupportsNodeInlineData<Ts...>::DestroyNodeInlineDataStorage() {
  storage_ = UnderlyingStorage();
}

template <class... Ts>
template <class T>
internal::Storage<T>& SupportsNodeInlineData<Ts...>::GetStorage() {
  return std::get<internal::Storage<T>>(storage_);
}

template <class... Ts>
template <class T>
const internal::Storage<T>& SupportsNodeInlineData<Ts...>::GetStorage() const {
  return std::get<internal::Storage<T>>(storage_);
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_INLINE_DATA_H_
