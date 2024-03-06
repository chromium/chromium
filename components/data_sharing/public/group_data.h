// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_

#include <string>

#include "url/gurl.h"

namespace data_sharing {

enum class MemberRole { kOwner, kMember, kInvitee };

struct GroupMember {
  GroupMember();

  GroupMember(const GroupMember&) = delete;
  GroupMember& operator=(const GroupMember&) = delete;

  GroupMember(GroupMember&&);
  GroupMember& operator=(GroupMember&&);

  ~GroupMember();

  std::string gaia_id;
  std::string display_name;
  MemberRole role;
  GURL avatar_url;
};

struct GroupData {
  GroupData();

  GroupData(const GroupData&) = delete;
  GroupData& operator=(const GroupData&) = delete;

  GroupData(GroupData&&);
  GroupData& operator=(GroupData&&);

  ~GroupData();

  std::string group_id;
  std::string display_name;
  std::vector<GroupMember> members;
};

// Only takes `group_id` into account, used to allow storing GroupData in
// std::set.
bool operator<(const GroupData& lhs, const GroupData& rhs);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
