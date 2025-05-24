// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_proto_utils.h"

#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/group_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_sharing {

namespace {

using testing::SizeIs;

constexpr char kGroupId[] = "group-id";
constexpr char kGroupDisplayName[] = "group-display-name";
constexpr char kGroupAccessToken[] = "group-access-token";

constexpr GaiaId::Literal kGaiaId1("gaia-id1");
constexpr char kUser1DisplayName[] = "user1-display-name";
constexpr char kEmail1[] = "user1@gmail.com";
constexpr char kAvatarUrl1[] = "https://google.com/avatar.png";

constexpr GaiaId::Literal kGaiaId2("gaia-id2");
constexpr char kUser2DisplayName[] = "user1-display-name";
constexpr char kEmail2[] = "user2@gmail.com";
constexpr char kAvatarUrl2[] = "https://google.com/avatar2.png";

data_sharing_pb::GroupMember MakeGroupMemberProto(
    const std::string& gaia_id,
    const std::string& display_name,
    const std::string& email,
    data_sharing_pb::MemberRole role,
    const std::string& avatar_url) {
  data_sharing_pb::GroupMember member_proto;
  member_proto.set_gaia_id(gaia_id);
  member_proto.set_display_name(display_name);
  member_proto.set_email(email);
  member_proto.set_role(role);
  member_proto.set_avatar_url(avatar_url);
  return member_proto;
}

TEST(GroupDataProtoUtilsTest, ShouldMakeGroupDataFromProto) {
  data_sharing_pb::GroupData group_data_proto;
  group_data_proto.set_group_id(kGroupId);
  group_data_proto.set_display_name(kGroupDisplayName);
  *group_data_proto.add_members() = MakeGroupMemberProto(
      kGaiaId1.ToString(), kUser1DisplayName, kEmail1,
      data_sharing_pb::MemberRole::MEMBER_ROLE_OWNER, kAvatarUrl1);
  *group_data_proto.add_former_members() = MakeGroupMemberProto(
      kGaiaId2.ToString(), kUser2DisplayName, kEmail2,
      data_sharing_pb::MemberRole::MEMBER_ROLE_FORMER_MEMBER, kAvatarUrl2);
  group_data_proto.set_access_token(kGroupAccessToken);
  group_data_proto.mutable_collaboration_group_metadata()->set_version(2);

  GroupData group_data = GroupDataFromProto(group_data_proto);

  EXPECT_EQ(group_data.group_token.group_id, GroupId(kGroupId));
  EXPECT_EQ(group_data.display_name, kGroupDisplayName);
  EXPECT_EQ(group_data.group_token.access_token, kGroupAccessToken);
  EXPECT_EQ(group_data.enabled_status,
            GroupEnabledStatus::kDisabledChromeNeedsUpdate);

  ASSERT_THAT(group_data.members, SizeIs(1));
  const GroupMember& member = group_data.members[0];
  EXPECT_EQ(member.gaia_id, kGaiaId1);
  EXPECT_EQ(member.display_name, kUser1DisplayName);
  EXPECT_EQ(member.email, kEmail1);
  EXPECT_EQ(member.role, MemberRole::kOwner);
  EXPECT_EQ(member.avatar_url.spec(), kAvatarUrl1);

  ASSERT_THAT(group_data.former_members, SizeIs(1));
  const GroupMember& forme_member = group_data.former_members[0];
  EXPECT_EQ(forme_member.gaia_id, kGaiaId2);
  EXPECT_EQ(forme_member.display_name, kUser2DisplayName);
  EXPECT_EQ(forme_member.email, kEmail2);
  EXPECT_EQ(forme_member.role, MemberRole::kFormerMember);
  EXPECT_EQ(forme_member.avatar_url.spec(), kAvatarUrl2);
}

}  // namespace

}  // namespace data_sharing
