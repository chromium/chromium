// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_STORAGE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_STORAGE_H_

#include <map>
#include <memory>

#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/graph/node_attached_data.h"

namespace performance_manager {

// Stores NodeAttachedData in nodes using NodeInlineData.
class NodeAttachedDataStorage : public NodeInlineData<NodeAttachedDataStorage> {
 public:
  NodeAttachedDataStorage();

  NodeAttachedDataStorage(const NodeAttachedDataStorage&) = delete;
  NodeAttachedDataStorage& operator=(const NodeAttachedDataStorage&) = delete;

  NodeAttachedDataStorage(NodeAttachedDataStorage&&);
  NodeAttachedDataStorage& operator=(NodeAttachedDataStorage&&);

  ~NodeAttachedDataStorage();

  // Attaches `data` to the node owning this storage.
  void AttachData(std::unique_ptr<NodeAttachedData> data);

  // Retrieves the data associated with `key`. Returns nullptr if there is none.
  NodeAttachedData* GetData(const void* key) const;

  // Detaches and returns the data associated with `key`. Returns nullptr if
  // there is none.
  std::unique_ptr<NodeAttachedData> DetachData(const void* key);

 private:
  std::map<const void*, std::unique_ptr<NodeAttachedData>>
      node_attached_data_map_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_STORAGE_H_
