// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_IMPL_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/performance_manager/graph/node_attached_data.h"

namespace performance_manager {

// Helper classes for defining a user data class that is associated with nodes
// in the graph. At most one instance of each type of data may exist per node
// in the graph.
//
// An instance of NodeAttachedData has to be specifically permitted to be
// associated with individual node types, with this being enforced at compile
// time via the type system. For each association, exactly one storage type is
// also specified. Storage may be one of the following:
//
// - In a singleton map owned by the graph (easiest, least efficient). This is
//   provided by deriving the traits from NodeAttachedDataInMap<>. If the data
//   is needed only for some instances of a node type, use this storage.
// - In a std::unique_ptr<NodeAttachedData> owned by the node itself
//   (slightly harder, slightly more efficient). The node type need only be
//   aware of the NodeAttachedData base class in this case. This is provided by
//   deriving the traits from NodeAttachedDataOwnedByNodeType<>. If the data is
//   needed for all instances of a node type, but only for a relatively small
//   portion of the nodes lifetime, use this storage type.
// - In a raw buffer owned by the node implementation, initialized by the
//   data type using a placement new (most difficult to use, but most memory
//   efficient). The node type needs to be aware of NodeAttachedData base class
//   and the size of the DataType in this case. This is provided by deriving
//   the traits from NodeAttachedDataInternalOnNodeType<>. If the data is needed
//   for all instances of a node type and for effectively the entire lifetime of
//   the node, use this storage type.
//
// These traits are provided by a "Traits" subclass, which is decorated by
// deriving from the appropriate storage type helpers. Keeping the decorations
// on a subclass keeps the data implementation class hierarchy clean, meaning it
// has a predictable size across all platforms and compilers (a single vtable
// entry).
//
// This is intended to be used as follows:
//
// -- foo.h --
// class Foo : public NodeAttachedDataImpl<Foo> {
//  public:
//   // This class communicates how the data type is allowed to be used in the
//   // graph.
//   struct Traits
//         // Can be associated with page nodes, storage in the map.
//       : public NodeAttachedDataInMap<PageNodeImpl>,
//         // Can be associated with frame nodes, storage provided by a
//         // std::unique_ptr<> member in the FrameNodeImpl.
//         public NodeAttachedDataOwnedByNodeType<FrameNodeImpl>,
//         // Can be associated with process nodes, storage provided inline
//         // in the ProcessNodeImpl.
//         public NodeAttachedDataInternalOnNodeType<ProcessNodeImpl> {};
//
//   ~Foo() override;
//
//  private:
//   // Make the impl our friend so it can access the constructor and any
//   // storage providers.
//   friend class ::performance_manager::NodeAttachedDataImpl<DataType>;
//
//   // For each node type that is supported a matching constructor must be
//   // available.
//   explicit Foo(const PageNodeImpl* page_node);
//   explicit Foo(const FrameNodeImpl* frame_node);
//   explicit Foo(const ProcessNodeImpl* process_node);
//
//   // Provides access to the std::unique_ptr storage in frame nodes.
//   // See NodeAttachedDataOwnedByNodeType<>.
//   static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
//       FrameNodeImpl* frame_node);
//
//   // Provides access to the inline storage in process nodes. The "4" must
//   // match sizeof(Foo). See NodeAttachedDataInternalOnNodeType<>.
//   static InternalNodeAttachedDataStorage<4>* GetInternalStorage(
//       ProcessNodeImpl* process_node);
//
// };
// -- foo.h --
//
// Once defined, the class is used the same way, regardless of where the
// storage is implemented. For each bound node type the following functions are
// exposed:
//
// - static Foo* Foo::GetOrCreate(const NodeType* node)
//   Creates a Foo associated with |node| if there isn't already one, and
//   returns it.
// - static Foo* Foo::Get(const NodeType* node)
//   Returns the Foo associated with |node|, which may be nullptr.
// - static bool Foo::Destroy(const NodeType* node)
//   Destroys the Foo associated with |node|, if one exists. Returns true if one
//   existed, false otherwise.
//
// For example:
//
// -- user_of_foo.cc --
// Foo* foo = Foo::GetOrCreate(page_node);
// foo->DoSomething();
// DHCECK_EQ(foo, Foo::Get(page_node));
// DCHECK(Foo::Destroy(page_node));
// -- user_of_foo.cc --

// Implementation of NodeAttachedData intended to be used as the base class for
// derived types. Provides the basic plumbing for accessing the node attached
// data in a strongly typed manner, while enforcing node type bindings.
template <typename DataType>
class NodeAttachedDataImpl : public NodeAttachedData {
 public:
  ///////////////////////////////////
  // Storage specification classes //
  ///////////////////////////////////

  // The pointer to this object acts as a unique key that identifies the type
  // at runtime. Note that the address of this should be taken only from a
  // single library, as a different address will be returned from each library
  // into which a given data type is linked.
  static constexpr int kUserDataKey = 0;

  // A class whose presence in the inheritance hierarchy of the Traits class
  // indicates that a NodeAttachedDataImpl is allowed to be attached to the
  // given node type, by type or by enum. Used by the storage impl classes.
  template <NodeTypeEnum kNodeType>
  class NodeAttachedDataPermittedByNodeTypeEnum {};
  template <typename NodeType>
  class NodeAttachedDataPermittedOnNodeType
      : public NodeAttachedDataPermittedByNodeTypeEnum<NodeType::Type()> {
    static_assert(std::is_base_of<NodeBase, NodeType>::value &&
                      !std::is_same<NodeBase, NodeType>::value,
                  "NodeType must be descended from NodeBase");
  };

  // The following 3 "mixin" classes are used to enable NodeAttachedData for a
  // given node type, and also to provide the storage type for the data. See
  // each class for details.

  // A "mixin" class that endows a NodeAttachedData implementation with strongly
  // typed data accessors for a given node type. This allows a NodeAttachedData
  // to be selectively bound only to certain node types. Use this with NodeBase
  // if the data can be attached to any graph node type.
  template <typename NodeType>
  class NodeAttachedDataInMap
      : public NodeAttachedDataPermittedOnNodeType<NodeType> {
   public:
    static DataType* GetOrCreate(const NodeType* node);
    static DataType* Get(const NodeType* node);
    static bool Destroy(const NodeType* node);
  };

  // A "mixin" class that endows a NodeAttachedData implementation with strongly
  // typed accessors for a given node type, where the storage for the data is
  // provided by a std::unique_ptr<NodeAttachedData> owned by the node.
  template <typename NodeType>
  class NodeAttachedDataOwnedByNodeType
      : public NodeAttachedDataPermittedOnNodeType<NodeType> {
   public:
    static DataType* GetOrCreate(const NodeType* node);
    static DataType* Get(const NodeType* node);
    static bool Destroy(const NodeType* node);
  };

  // A "mixin" class that endows a NodeAttachedData implementation with strongly
  // typed accessors for a given node type, where the storage for the data is
  // provided by an InternalNodeAttachedDataStorage<> on the node.
  template <typename NodeType>
  class NodeAttachedDataInternalOnNodeType
      : public NodeAttachedDataPermittedOnNodeType<NodeType> {
   public:
    static DataType* GetOrCreate(const NodeType* node);
    static DataType* Get(const NodeType* node);
    static bool Destroy(const NodeType* node);
  };

  static const void* UserDataKey() { return &DataType::kUserDataKey; }

  // NodeAttachedData implementation:
  const void* GetKey() const override { return UserDataKey(); }

 private:
  // Uses implicit conversion of the traits to get the appropriate mixin class
  // that implements storage for the node type. This is used to provide dispatch
  // of GetOrCreate/Get/Destroy base on the storage type.
  template <class NodeType>
  static const NodeAttachedDataInMap<NodeType>& GetTraits(
      NodeAttachedDataInMap<NodeType>& traits) {
    return traits;
  }
  template <class NodeType>
  static const NodeAttachedDataOwnedByNodeType<NodeType>& GetTraits(
      NodeAttachedDataOwnedByNodeType<NodeType>& traits) {
    return traits;
  }
  template <class NodeType>
  static const NodeAttachedDataInternalOnNodeType<NodeType>& GetTraits(
      NodeAttachedDataInternalOnNodeType<NodeType>& traits) {
    return traits;
  }

 public:
  // Creates (if necessary) and retrieves the data associated with the provided
  // node.
  template <typename NodeType>
  static DataType* GetOrCreate(const NodeType* node) {
    typename DataType::Traits traits;
    return GetTraits<NodeType>(traits).GetOrCreate(node);
  }

  // Retrieves the data associated with the provided node if it exists.
  template <typename NodeType>
  static DataType* Get(const NodeType* node) {
    typename DataType::Traits traits;
    return GetTraits<NodeType>(traits).Get(node);
  }

  // Destroys the data associated with the provided node. Returns true data was
  // deleted, false otherwise.
  template <typename NodeType>
  static bool Destroy(const NodeType* node) {
    typename DataType::Traits traits;
    return GetTraits<NodeType>(traits).Destroy(node);
  }
};

// static
template <typename DataType>
constexpr int NodeAttachedDataImpl<DataType>::kUserDataKey;

///////////////////////////////////////////////////////////
// Everything below this point is implementation detail! //
///////////////////////////////////////////////////////////

// Helper class allowing access to internals of
// InternalNodeAttachedDataStorage<>.
class InternalNodeAttachedDataStorageAccess {
 public:
  // InternalNodeAttachedDataStorage<> forwarding.
  template <typename InlineStorageType>
  static void Set(InlineStorageType* storage, NodeAttachedData* data) {
    storage->Set(data);
  }
};

// Implementation of storage type mixins.

// Map storage impl.

// static
template <typename DataType>
template <typename NodeType>
DataType*
NodeAttachedDataImpl<DataType>::NodeAttachedDataInMap<NodeType>::GetOrCreate(
    const NodeType* node) {
  if (auto* data = Get(node))
    return data;
  std::unique_ptr<DataType> data = base::WrapUnique(new DataType(node));
  DataType* raw_data = data.get();
  NodeAttachedDataMapHelper::AttachInMap(node, std::move(data));
  return raw_data;
}

// static
template <typename DataType>
template <typename NodeType>
DataType* NodeAttachedDataImpl<DataType>::NodeAttachedDataInMap<NodeType>::Get(
    const NodeType* node) {
  auto* data =
      NodeAttachedDataMapHelper::GetFromMap(node, DataType::UserDataKey());
  DCHECK(!data || DataType::UserDataKey() == data->GetKey());
  return static_cast<DataType*>(data);
}

// static
template <typename DataType>
template <typename NodeType>
bool NodeAttachedDataImpl<DataType>::NodeAttachedDataInMap<NodeType>::Destroy(
    const NodeType* node) {
  std::unique_ptr<NodeAttachedData> data =
      NodeAttachedDataMapHelper::DetachFromMap(node, DataType::UserDataKey());
  return data.get();
}

// Node owned storage impl.

// static
template <typename DataType>
template <typename NodeType>
DataType* NodeAttachedDataImpl<DataType>::NodeAttachedDataOwnedByNodeType<
    NodeType>::GetOrCreate(const NodeType* node) {
  std::unique_ptr<NodeAttachedData>* storage =
      DataType::GetUniquePtrStorage(const_cast<NodeType*>(node));
  if (!storage->get())
    *storage = base::WrapUnique(new DataType(node));
  DCHECK_EQ(DataType::UserDataKey(), storage->get()->GetKey());
  return static_cast<DataType*>(storage->get());
}

// static
template <typename DataType>
template <typename NodeType>
DataType*
NodeAttachedDataImpl<DataType>::NodeAttachedDataOwnedByNodeType<NodeType>::Get(
    const NodeType* node) {
  std::unique_ptr<NodeAttachedData>* storage =
      DataType::GetUniquePtrStorage(const_cast<NodeType*>(node));
  if (storage->get())
    DCHECK_EQ(DataType::UserDataKey(), storage->get()->GetKey());
  return static_cast<DataType*>(storage->get());
}

// static
template <typename DataType>
template <typename NodeType>
bool NodeAttachedDataImpl<DataType>::NodeAttachedDataOwnedByNodeType<
    NodeType>::Destroy(const NodeType* node) {
  std::unique_ptr<NodeAttachedData>* storage =
      DataType::GetUniquePtrStorage(const_cast<NodeType*>(node));
  bool data_exists = storage->get();
  storage->reset();
  return data_exists;
}

// Node internal storage impl.

// static
template <typename DataType>
template <typename NodeType>
DataType* NodeAttachedDataImpl<DataType>::NodeAttachedDataInternalOnNodeType<
    NodeType>::GetOrCreate(const NodeType* node) {
  // TODO(chrisha): Add a compile test that this is enforced. Otherwise, there's
  // potential for a OOB reads / security issues. https://www.crbug.com/952864
  InternalNodeAttachedDataStorage<sizeof(DataType)>* storage =
      DataType::GetInternalStorage(const_cast<NodeType*>(node));
  if (!storage->Get()) {
    NodeAttachedData* data = new (storage->buffer()) DataType(node);
    InternalNodeAttachedDataStorageAccess::Set(storage, data);
  }
  DCHECK_EQ(DataType::UserDataKey(), storage->Get()->GetKey());
  return static_cast<DataType*>(storage->Get());
}

// static
template <typename DataType>
template <typename NodeType>
DataType* NodeAttachedDataImpl<DataType>::NodeAttachedDataInternalOnNodeType<
    NodeType>::Get(const NodeType* node) {
  InternalNodeAttachedDataStorage<sizeof(DataType)>* storage =
      DataType::GetInternalStorage(const_cast<NodeType*>(node));
  if (storage->Get())
    DCHECK_EQ(DataType::UserDataKey(), storage->Get()->GetKey());
  return static_cast<DataType*>(storage->Get());
}

// static
template <typename DataType>
template <typename NodeType>
bool NodeAttachedDataImpl<DataType>::NodeAttachedDataInternalOnNodeType<
    NodeType>::Destroy(const NodeType* node) {
  InternalNodeAttachedDataStorage<sizeof(DataType)>* storage =
      DataType::GetInternalStorage(const_cast<NodeType*>(node));
  bool data_exists = storage->Get();
  storage->Reset();
  return data_exists;
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_IMPL_H_
