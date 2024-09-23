// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_attached_data.h"

#include <utility>

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/node_attached_data_storage.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/node.h"

namespace performance_manager {

namespace {

template <class NodeClassImpl>
NodeAttachedDataStorage& GetStorageImpl(NodeClassImpl* node) {
  return NodeAttachedDataStorage::Get(node);
}

NodeAttachedDataStorage& GetStorage(const Node* node) {
  switch (node->GetNodeType()) {
    case NodeTypeEnum::kProcess:
      return GetStorageImpl(ProcessNodeImpl::FromNode(node));
    case NodeTypeEnum::kPage:
      return GetStorageImpl(PageNodeImpl::FromNode(node));
    case NodeTypeEnum::kFrame:
      return GetStorageImpl(FrameNodeImpl::FromNode(node));
    case NodeTypeEnum::kWorker:
      return GetStorageImpl(WorkerNodeImpl::FromNode(node));
    case NodeTypeEnum::kSystem:
      NOTREACHED();
  }
  NOTREACHED();
}

}  // namespace

NodeAttachedData::NodeAttachedData() = default;

NodeAttachedData::~NodeAttachedData() = default;

// static
void NodeAttachedDataMapHelper::AttachInMap(
    const Node* node,
    std::unique_ptr<NodeAttachedData> data) {
  GetStorage(node).AttachData(std::move(data));
}

// static
NodeAttachedData* NodeAttachedDataMapHelper::GetFromMap(const Node* node,
                                                        const void* key) {
  return GetStorage(node).GetData(key);
}

// static
std::unique_ptr<NodeAttachedData> NodeAttachedDataMapHelper::DetachFromMap(
    const Node* node,
    const void* key) {
  return GetStorage(node).DetachData(key);
}

}  // namespace performance_manager
