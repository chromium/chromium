// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/node_attached_data_storage.h"

#include <utility>

#include "base/check_op.h"

namespace performance_manager {

NodeAttachedDataStorage::NodeAttachedDataStorage() = default;

NodeAttachedDataStorage::NodeAttachedDataStorage(NodeAttachedDataStorage&&) =
    default;

NodeAttachedDataStorage& NodeAttachedDataStorage::operator=(
    NodeAttachedDataStorage&&) = default;

NodeAttachedDataStorage::~NodeAttachedDataStorage() = default;

void NodeAttachedDataStorage::AttachData(
    std::unique_ptr<NodeAttachedData> data) {
  const void* key = data->GetKey();
  auto [_, inserted] =
      node_attached_data_map_.try_emplace(key, std::move(data));
  CHECK(inserted);
}

NodeAttachedData* NodeAttachedDataStorage::GetData(const void* key) const {
  auto it = node_attached_data_map_.find(key);
  if (it == node_attached_data_map_.end()) {
    return nullptr;
  }
  CHECK_EQ(key, it->second->GetKey());
  return it->second.get();
}

std::unique_ptr<NodeAttachedData> NodeAttachedDataStorage::DetachData(
    const void* key) {
  auto it = node_attached_data_map_.find(key);

  std::unique_ptr<NodeAttachedData> data;
  if (it != node_attached_data_map_.end()) {
    data = std::move(it->second);
    node_attached_data_map_.erase(it);
  }

  return data;
}

}  // namespace performance_manager
