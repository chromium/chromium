// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_

#include <string>

#include "base/types/strong_alias.h"
#include "url/gurl.h"

namespace data_sharing {

using GroupId = base::StrongAlias<class GroupIdTag, std::string>;

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.data_sharing.member_role)
enum class MemberRole { kUnknown = 0, kOwner = 1, kMember = 2, kInvitee = 3 };

struct GroupMember {
  GroupMember();

  GroupMember(const GroupMember&);
  GroupMember& operator=(const GroupMember&);

  GroupMember(GroupMember&&);
  GroupMember& operator=(GroupMember&&);

  ~GroupMember();

  std::string gaia_id;
  std::string display_name;
  std::string email;
  MemberRole role;
  GURL avatar_url;
};

struct GroupData {
  GroupData();

  GroupData(const GroupData&);
  GroupData& operator=(const GroupData&);

  GroupData(GroupData&&);
  GroupData& operator=(GroupData&&);

  ~GroupData();

  GroupId group_id;
  std::string display_name;
  std::vector<GroupMember> members;
};

// Only takes `group_id` into account, used to allow storing GroupData in
// std::set.
bool operator<(const GroupData& lhs, const GroupData& rhs);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
