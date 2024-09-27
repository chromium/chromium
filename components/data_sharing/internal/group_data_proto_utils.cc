// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_proto_utils.h"

#include "base/notreached.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/group_data.pb.h"

namespace data_sharing {

namespace {

data_sharing_pb::MemberRole MemberRoleToProto(const MemberRole& member_role) {
  switch (member_role) {
    case MemberRole::kOwner:
      return data_sharing_pb::MEMBER_ROLE_OWNER;
    case MemberRole::kMember:
      return data_sharing_pb::MEMBER_ROLE_MEMBER;
    case MemberRole::kInvitee:
      return data_sharing_pb::MEMBER_ROLE_INVITEE;
    case MemberRole::kUnknown:
      return data_sharing_pb::MEMBER_ROLE_UNSPECIFIED;
  }
  NOTREACHED();
}

MemberRole MemberRoleFromProto(const data_sharing_pb::MemberRole& member_role) {
  switch (member_role) {
    case data_sharing_pb::MEMBER_ROLE_OWNER:
      return MemberRole::kOwner;
    case data_sharing_pb::MEMBER_ROLE_MEMBER:
      return MemberRole::kMember;
    case data_sharing_pb::MEMBER_ROLE_INVITEE:
      return MemberRole::kInvitee;
    case data_sharing_pb::MEMBER_ROLE_UNSPECIFIED:
      return MemberRole::kUnknown;
  }
  NOTREACHED();
}

data_sharing_pb::GroupMember GroupMemberToProto(
    const GroupMember& group_member) {
  data_sharing_pb::GroupMember result;
  result.set_gaia_id(group_member.gaia_id);
  result.set_display_name(group_member.display_name);
  result.set_email(group_member.email);
  result.set_role(MemberRoleToProto(group_member.role));
  result.set_avatar_url(group_member.avatar_url.spec());
  result.set_given_name(group_member.given_name);
  return result;
}

GroupMember GroupMemberFromProto(
    const data_sharing_pb::GroupMember& group_member_proto) {
  GroupMember result;
  result.gaia_id = group_member_proto.gaia_id();
  result.display_name = group_member_proto.display_name();
  result.email = group_member_proto.email();
  result.role = MemberRoleFromProto(group_member_proto.role());
  result.avatar_url = GURL(group_member_proto.avatar_url());
  result.given_name = group_member_proto.given_name();
  return result;
}

}  // namespace

data_sharing_pb::GroupData GroupDataToProto(const GroupData& group_data) {
  data_sharing_pb::GroupData result;
  result.set_group_id(group_data.group_token.group_id.value());
  result.set_display_name(group_data.display_name);
  for (const auto& member : group_data.members) {
    *result.add_members() = GroupMemberToProto(member);
  }
  result.set_access_token(group_data.group_token.access_token);
  return result;
}

GroupData GroupDataFromProto(
    const data_sharing_pb::GroupData& group_data_proto) {
  GroupData result;
  result.group_token.group_id = GroupId(group_data_proto.group_id());
  result.display_name = group_data_proto.display_name();
  for (const auto& member_proto : group_data_proto.members()) {
    result.members.push_back(GroupMemberFromProto(member_proto));
  }
  result.group_token.access_token = group_data_proto.access_token();
  return result;
}

}  // namespace data_sharing
