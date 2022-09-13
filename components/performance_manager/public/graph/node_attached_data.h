// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_ATTACHED_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_ATTACHED_DATA_H_

#include <memory>

#include "base/check_op.h"

namespace performance_manager {

class Node;

// NodeAttachedData allows external observers of the graph to store data that is
// associated with a graph node, providing lifetime management as a service.
//
// External (to performance_manager) implementations of NodeAttachedData should
// derive from ExternalNodeAttachedDataImpl. For internal implementations refer
// to NodeAttachedDataImpl and see node_attached_data_impl.h.
class NodeAttachedData {
 public:
  NodeAttachedData();

  NodeAttachedData(const NodeAttachedData&) = delete;
  NodeAttachedData& operator=(const NodeAttachedData&) = delete;

  virtual ~NodeAttachedData();

  // Returns the 'key' associated with this type of NodeAttachedData. This needs
  // to be unique per data type or bad things happen.
  virtual const void* GetKey() const = 0;
};

// Implements NodeAttachedData for a given UserDataType.
//
// In order for a UserDataType to be attached to a node of type |NodeType| it
// must have a constructor of the form "UserDataType(const NodeType* node)".
template <typename UserDataType>
class ExternalNodeAttachedDataImpl : public NodeAttachedData {
 public:
  ExternalNodeAttachedDataImpl() = default;

  ExternalNodeAttachedDataImpl(const ExternalNodeAttachedDataImpl&) = delete;
  ExternalNodeAttachedDataImpl& operator=(const ExternalNodeAttachedDataImpl&) =
      delete;

  ~ExternalNodeAttachedDataImpl() override = default;

  // NodeAttachedData implementation:
  const void* GetKey() const override { return &kUserDataKey; }

  // Gets the user data for the given |node|, creating it if it doesn't yet
  // exist.
  template <typename NodeType>
  static UserDataType* GetOrCreate(const NodeType* node);

  // Gets the user data for the given |node|, returning nullptr if it doesn't
  // exist.
  template <typename NodeType>
  static UserDataType* Get(const NodeType* node);

  // Destroys the user data associated with the given node, returning true
  // on success or false if the user data did not exist to begin with.
  template <typename NodeType>
  static bool Destroy(const NodeType* node);

 private:
  static const int kUserDataKey = 0;
  static const void* UserDataKey() { return &kUserDataKey; }
};

// Everything below this point is pure implementation detail.

// Provides access for setting/getting data in the map based storage. Not
// intended to be used directly, but rather by (External)NodeAttachedDataImpl.
class NodeAttachedDataMapHelper {
 public:
  // Attaches the provided |data| to the provided |node|. This should only be
  // called if the data does not exist (GetFromMap returns nullptr), and will
  // DCHECK otherwise.
  static void AttachInMap(const Node* node,
                          std::unique_ptr<NodeAttachedData> data);

  // Retrieves the data associated with the provided |node| and |key|. This
  // returns nullptr if no data exists.
  static NodeAttachedData* GetFromMap(const Node* node, const void* key);

  // Detaches the data associated with the provided |node| and |key|. It is
  // harmless to call this when no data exists.
  static std::unique_ptr<NodeAttachedData> DetachFromMap(const Node* node,
                                                         const void* key);
};

// Implementation of ExternalNodeAttachedDataImpl, which is declared in the
// corresponding public header. This helps keep the public headers as clean as
// possible.

// static
template <typename UserDataType>
constexpr int ExternalNodeAttachedDataImpl<UserDataType>::kUserDataKey;

template <typename UserDataType>
template <typename NodeType>
UserDataType* ExternalNodeAttachedDataImpl<UserDataType>::GetOrCreate(
    const NodeType* node) {
  if (auto* data = Get(node))
    return data;
  std::unique_ptr<UserDataType> data = std::make_unique<UserDataType>(node);
  auto* raw_data = data.get();
  auto* base = static_cast<const Node*>(node);
  NodeAttachedDataMapHelper::AttachInMap(base, std::move(data));
  return raw_data;
}

template <typename UserDataType>
template <typename NodeType>
UserDataType* ExternalNodeAttachedDataImpl<UserDataType>::Get(
    const NodeType* node) {
  auto* base = static_cast<const Node*>(node);
  auto* data = NodeAttachedDataMapHelper::GetFromMap(base, UserDataKey());
  if (!data)
    return nullptr;
  DCHECK_EQ(UserDataKey(), data->GetKey());
  return static_cast<UserDataType*>(data);
}

template <typename UserDataType>
template <typename NodeType>
bool ExternalNodeAttachedDataImpl<UserDataType>::Destroy(const NodeType* node) {
  auto* base = static_cast<const Node*>(node);
  std::unique_ptr<NodeAttachedData> data =
      NodeAttachedDataMapHelper::DetachFromMap(base, UserDataKey());
  return data.get();
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_ATTACHED_DATA_H_
