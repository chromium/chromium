// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_proto_utils.h"

#include "base/notreached.h"
#include "components/data_sharing/public/client_version_info.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/group_data.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace data_sharing {

namespace {

MemberRole MemberRoleFromProto(const data_sharing_pb::MemberRole& member_role) {
  switch (member_role) {
    case data_sharing_pb::MEMBER_ROLE_OWNER:
      return MemberRole::kOwner;
    case data_sharing_pb::MEMBER_ROLE_MEMBER:
      return MemberRole::kMember;
    case data_sharing_pb::MEMBER_ROLE_INVITEE:
      return MemberRole::kInvitee;
    case data_sharing_pb::MEMBER_ROLE_FORMER_MEMBER:
      return MemberRole::kFormerMember;
    case data_sharing_pb::MEMBER_ROLE_UNSPECIFIED:
      return MemberRole::kUnknown;
  }
  NOTREACHED();
}

GroupMember GroupMemberFromProto(
    const data_sharing_pb::GroupMember& group_member_proto) {
  GroupMember result;
  result.gaia_id = GaiaId(group_member_proto.gaia_id());
  result.display_name = group_member_proto.display_name();
  result.email = group_member_proto.email();
  result.role = MemberRoleFromProto(group_member_proto.role());
  result.avatar_url = GURL(group_member_proto.avatar_url());
  result.given_name = group_member_proto.given_name();
  result.creation_time = base::Time::FromMillisecondsSinceUnixEpoch(
      group_member_proto.creation_time_unix_epoch_millis());
  result.last_updated_time = base::Time::FromMillisecondsSinceUnixEpoch(
      group_member_proto.last_updated_time_unix_epoch_millis());
  return result;
}

}  // namespace

GroupData GroupDataFromProto(
    const data_sharing_pb::GroupData& group_data_proto) {
  GroupData result;
  result.group_token.group_id = GroupId(group_data_proto.group_id());
  result.display_name = group_data_proto.display_name();
  for (const auto& member_proto : group_data_proto.members()) {
    if (member_proto.role() != data_sharing_pb::MEMBER_ROLE_OWNER &&
        member_proto.role() != data_sharing_pb::MEMBER_ROLE_MEMBER) {
      continue;
    }
    result.members.push_back(GroupMemberFromProto(member_proto));
  }
  for (const auto& member_proto : group_data_proto.former_members()) {
    if (member_proto.role() != data_sharing_pb::MEMBER_ROLE_FORMER_MEMBER) {
      continue;
    }
    result.former_members.push_back(GroupMemberFromProto(member_proto));
  }
  result.group_token.access_token = group_data_proto.access_token();

  if (group_data_proto.has_collaboration_group_metadata()) {
    int64_t group_version =
        group_data_proto.collaboration_group_metadata().version();
    int64_t client_version =
        static_cast<int64_t>(ClientVersionInfo::CURRENT_VERSION);
    result.enabled_status =
        (group_version <= client_version)
            ? GroupEnabledStatus::kEnabled
            : GroupEnabledStatus::kDisabledChromeNeedsUpdate;
  }
  return result;
}

}  // namespace data_sharing
