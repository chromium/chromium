// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_STORE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_STORE_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/types/strong_alias.h"
#include "components/data_sharing/public/group_data.h"

namespace data_sharing {

// TODO(crbug.com/301390275): figure out what precisely this should be
// (ConsistencyToken, timestamp, etc.).
using VersionToken = base::StrongAlias<class VersionTokenTag, std::string>;

// In-memory cache and persistent storage for GroupData.
// TODO(crbug.com/301390275): support storing data in SQLite DB.
class GroupDataStore {
 public:
  GroupDataStore();
  ~GroupDataStore();

  void StoreGroupData(const VersionToken& version_token,
                      const GroupData& group_data);
  void DeleteGroupData(const GroupId& group_id);

  std::optional<VersionToken> GetGroupVersionToken(
      const GroupId& group_id) const;
  std::optional<GroupData> GetGroupData(const GroupId& group_id) const;
  std::vector<GroupId> GetAllGroupsIds() const;

 private:
  using GroupIdToDataMap =
      std::unordered_map<GroupId, GroupData, GroupId::Hasher>;
  using GroupIdToVersionTokenMap =
      std::unordered_map<GroupId, VersionToken, GroupId::Hasher>;

  GroupIdToDataMap group_id_to_data_;
  GroupIdToVersionTokenMap group_id_to_version_token_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_STORE_H_
