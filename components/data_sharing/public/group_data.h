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

struct GroupToken {
  GroupToken();

  GroupToken(GroupId group_id, std::string access_token);

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

// Represents an entity that is shared between users. This
// is similar to sync_pb::SyncEntity, but it includes group
// ID and is only for shared data types
struct SharedEntity {
  SharedEntity();

  SharedEntity(const SharedEntity&);
  SharedEntity& operator=(const SharedEntity&);

  SharedEntity(SharedEntity&&);
  SharedEntity& operator=(SharedEntity&&);

  ~SharedEntity();

  // Id of the group.
  GroupId group_id;

  // Name of the entity.
  std::string name;

  // Monotonically increasing version number.
  int64_t version = 0;

  // The time at which the SharedEntity was last modified.
  base::Time update_time;

  // The time at which the SharedEntity was created.
  base::Time create_time;

  // The data payload of the SharedEntity.
  sync_pb::EntitySpecifics specifics;

  // Part of the resource name.
  std::string client_tag_hash;
};

// A preview of shared entities.
struct SharedDataPreview {
  SharedDataPreview();

  SharedDataPreview(const SharedDataPreview&);
  SharedDataPreview& operator=(const SharedDataPreview&);

  SharedDataPreview(SharedDataPreview&&);
  SharedDataPreview& operator=(SharedDataPreview&&);

  ~SharedDataPreview();

  std::vector<SharedEntity> shared_entities;
};

// Only takes `group_id` into account, used to allow storing GroupData in
// std::set.
bool operator<(const GroupData& lhs, const GroupData& rhs);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_GROUP_DATA_H_
