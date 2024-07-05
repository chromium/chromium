// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_store.h"

#include <optional>
#include <string>
#include <vector>

namespace data_sharing {

GroupDataStore::GroupDataStore() = default;
GroupDataStore::~GroupDataStore() = default;

void GroupDataStore::StoreGroupData(const VersionToken& version_token,
                                    const GroupData& group_data) {
  group_id_to_data_[group_data.group_id] = group_data;
  group_id_to_version_token_[group_data.group_id] = version_token;
}

void GroupDataStore::DeleteGroupData(const GroupId& group_id) {
  group_id_to_data_.erase(group_id);
  group_id_to_version_token_.erase(group_id);
}

std::optional<VersionToken> GroupDataStore::GetGroupVersionToken(
    const GroupId& group_id) const {
  auto it = group_id_to_version_token_.find(group_id);
  if (it == group_id_to_version_token_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<GroupData> GroupDataStore::GetGroupData(
    const GroupId& group_id) const {
  auto it = group_id_to_data_.find(group_id);
  if (it == group_id_to_data_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<GroupId> GroupDataStore::GetAllGroupsIds() const {
  std::vector<GroupId> result;
  for (const auto& [group_id, _] : group_id_to_data_) {
    result.push_back(group_id);
  }
  return result;
}

}  // namespace data_sharing
