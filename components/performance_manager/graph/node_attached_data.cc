// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_attached_data.h"

#include <utility>

#include "base/logging.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

NodeAttachedData::NodeAttachedData() = default;
NodeAttachedData::~NodeAttachedData() = default;

// static
void NodeAttachedDataMapHelper::AttachInMap(
    const Node* node,
    std::unique_ptr<NodeAttachedData> data) {
  GraphImpl* graph = GraphImpl::FromGraph(node->GetGraph());
  const NodeBase* node_base = NodeBase::FromNode(node);
  DCHECK(graph->NodeInGraph(node_base));
  GraphImpl::NodeAttachedDataKey data_key =
      std::make_pair(node, data->GetKey());
  auto& map = graph->node_attached_data_map_;
  DCHECK(!base::Contains(map, data_key));
  map[data_key] = std::move(data);
}

// static
NodeAttachedData* NodeAttachedDataMapHelper::GetFromMap(const Node* node,
                                                        const void* key) {
  GraphImpl* graph = GraphImpl::FromGraph(node->GetGraph());
  const NodeBase* node_base = NodeBase::FromNode(node);
  DCHECK(graph->NodeInGraph(node_base));
  GraphImpl::NodeAttachedDataKey data_key = std::make_pair(node, key);
  auto& map = graph->node_attached_data_map_;
  auto it = map.find(data_key);
  if (it == map.end())
    return nullptr;
  DCHECK_EQ(key, it->second->GetKey());
  return it->second.get();
}

// static
std::unique_ptr<NodeAttachedData> NodeAttachedDataMapHelper::DetachFromMap(
    const Node* node,
    const void* key) {
  GraphImpl* graph = GraphImpl::FromGraph(node->GetGraph());
  const NodeBase* node_base = NodeBase::FromNode(node);
  DCHECK(graph->NodeInGraph(node_base));
  GraphImpl::NodeAttachedDataKey data_key = std::make_pair(node, key);
  auto& map = graph->node_attached_data_map_;
  auto it = map.find(data_key);

  std::unique_ptr<NodeAttachedData> data;
  if (it != map.end()) {
    data = std::move(it->second);
    map.erase(it);
  }

  return data;
}

}  // namespace performance_manager
