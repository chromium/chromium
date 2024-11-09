// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_

#include <string>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/sync/protocol/entity_specifics.pb.h"
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
  std::string given_name;
};

// Subset of GroupMember fields that could be temporarily stored after member is
// removed from the group.
struct GroupMemberPartialData {
  static GroupMemberPartialData FromGroupMember(const GroupMember& member);

  GroupMemberPartialData();

  GroupMemberPartialData(const GroupMemberPartialData&);
  GroupMemberPartialData& operator=(const GroupMemberPartialData&);

  GroupMemberPartialData(GroupMemberPartialData&&);
  GroupMemberPartialData& operator=(GroupMemberPartialData&&);

  ~GroupMemberPartialData();

  std::string gaia_id;
  std::string display_name;
  std::string email;
  GURL avatar_url;
};

struct GroupToken {
  GroupToken();

  GroupToken(GroupId group_id, const std::string& access_token);

  GroupToken(const GroupToken&);
  GroupToken& operator=(const GroupToken&);

  GroupToken(GroupToken&&);
  GroupToken& operator=(GroupToken&&);

  ~GroupToken();

  bool IsValid() const;

  GroupId group_id;
  std::string access_token;
};

struct GroupData {
  GroupData();

  GroupData(GroupId group_id,
            std::string display_name,
            std::vector<GroupMember> members,
            std::string access_token);

  GroupData(const GroupData&);
  GroupData& operator=(const GroupData&);

  GroupData(GroupData&&);
  GroupData& operator=(GroupData&&);

  ~GroupData();

  GroupToken group_token;
  std::string display_name;
  std::vector<GroupMember> members;
};

// Represents a tab that is shared in a group.
struct TabPreview {
  explicit TabPreview(const GURL& url);

  TabPreview(const TabPreview&);
  TabPreview& operator=(const TabPreview&);

  TabPreview(TabPreview&&);
  TabPreview& operator=(TabPreview&&);

  ~TabPreview();

  // Trim the tab url to display url. E.g.
  // "https://www.google.com/search?q=wiki" to "google.com".
  std::string GetDisplayUrl() const;

  // URL of the tab.
  GURL url;
};

// Represents a tab group that is shared between users.
struct SharedTabGroupPreview {
  SharedTabGroupPreview();

  SharedTabGroupPreview(const SharedTabGroupPreview&);
  SharedTabGroupPreview& operator=(const SharedTabGroupPreview&);

  SharedTabGroupPreview(SharedTabGroupPreview&&);
  SharedTabGroupPreview& operator=(SharedTabGroupPreview&&);

  ~SharedTabGroupPreview();

  // Title of the group.
  std::string title;

  // All tabs in the group, ordered by their UniquePosition.
  std::vector<TabPreview> tabs;
};

// A preview of shared data.
struct SharedDataPreview {
  SharedDataPreview();

  SharedDataPreview(const SharedDataPreview&);
  SharedDataPreview& operator=(const SharedDataPreview&);

  SharedDataPreview(SharedDataPreview&&);
  SharedDataPreview& operator=(SharedDataPreview&&);

  ~SharedDataPreview();

  // Shared tab group data.
  std::optional<SharedTabGroupPreview> shared_tab_group_preview;
};

// Only takes `group_id` into account, used to allow storing GroupData in
// std::set.
bool operator<(const GroupData& lhs, const GroupData& rhs);

// Used to allow storing GroupToken in arrays.
bool operator==(const GroupToken& lhs, const GroupToken& rhs);
bool operator<(const GroupToken& lhs, const GroupToken& rhs);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
