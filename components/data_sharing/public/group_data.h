// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_

#include <string>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "google_apis/gaia/gaia_id.h"
#include "url/gurl.h"

namespace data_sharing {

using GroupId = base::StrongAlias<class GroupIdTag, std::string>;

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.data_sharing.member_role)
enum class MemberRole {
  kUnknown = 0,
  kOwner = 1,
  kMember = 2,
  kInvitee = 3,
  kFormerMember = 4
};

// This tells if the group is enabled or not. This field is set by chrome client
// after comparing the version info from ReadGroup request and comparing it with
// hardcoded version info in Chrome client.
enum class GroupEnabledStatus {
  kUnknown = 0,
  kEnabled = 1,
  kDisabledChromeNeedsUpdate = 2,
};

struct GroupMember {
  GroupMember();

  GroupMember(GaiaId gaia_id,
              std::string display_name,
              std::string email,
              MemberRole role,
              GURL avatar_url,
              std::string given_name);

  GroupMember(const GroupMember&);
  GroupMember& operator=(const GroupMember&);

  GroupMember(GroupMember&&);
  GroupMember& operator=(GroupMember&&);

  ~GroupMember();

  GaiaId gaia_id;
  std::string display_name;
  std::string email;
  MemberRole role = MemberRole::kUnknown;
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

  GroupMember ToGroupMember();

  GaiaId gaia_id;
  std::string display_name;
  std::string email;
  GURL avatar_url;
  std::string given_name;
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
            std::vector<GroupMember> former_members,
            std::string access_token,
            GroupEnabledStatus enabled_status = GroupEnabledStatus::kEnabled);

  GroupData(const GroupData&);
  GroupData& operator=(const GroupData&);

  GroupData(GroupData&&);
  GroupData& operator=(GroupData&&);

  ~GroupData();

  GroupToken group_token;
  std::string display_name;
  std::vector<GroupMember> members;
  std::vector<GroupMember> former_members;
  GroupEnabledStatus enabled_status = GroupEnabledStatus::kEnabled;
};

struct GroupEvent {
  enum class EventType {
    kGroupAdded,
    kGroupRemoved,
    kMemberRemoved,
    kMemberAdded,
  };

  GroupEvent();

  GroupEvent(const GroupEvent&);
  GroupEvent& operator=(const GroupEvent&);

  GroupEvent(GroupEvent&&);
  GroupEvent& operator=(GroupEvent&&);

  GroupEvent(EventType event_type,
             const GroupId& group_id,
             const std::optional<GaiaId>& affected_member_gaia_id,
             const base::Time& event_time);

  ~GroupEvent();

  EventType event_type;
  GroupId group_id;
  // Unset for kGroupAdded and kGroupRemoved events.
  std::optional<GaiaId> affected_member_gaia_id;
  base::Time event_time;
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

// The state of the sync bridge wrt sign-in / sign-out, i.e. whether the bridge
// has completed initial merge and isn't in the process of disabling sync.
// Interested consumers might want to ignore the incoming updates from sync
// based on this enum.
enum class SyncBridgeUpdateType {
  // The bridge is currently undergoing initial merge. After this stage, it will
  // transition to `kDefaultState`.
  kInitialMerge = 0,

  // The bridge is currently in the process of disabling, i.e.
  // ApplyDisableSyncChanges has been invoked. After this stage, it will
  // transition to `kDefaultState`.
  kDisableSync = 1,

  // The bridge is not currently doing an initial merge or disable sync
  // operation.
  kDefaultState = 2,
};

// Only takes `group_id` into account, used to allow storing GroupData in
// std::set.
bool operator<(const GroupData& lhs, const GroupData& rhs);

// Used to allow storing GroupToken in arrays.
bool operator==(const GroupToken& lhs, const GroupToken& rhs);
bool operator<(const GroupToken& lhs, const GroupToken& rhs);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
