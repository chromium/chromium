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

constexpr char kGaiaId1[] = "gaia-id1";
constexpr char kUser1DisplayName[] = "user1-display-name";
constexpr char kEmail1[] = "user1@gmail.com";
constexpr char kAvatarUrl1[] = "https://google.com/avatar.png";

constexpr char kGaiaId2[] = "gaia-id2";
constexpr char kUser2DisplayName[] = "user1-display-name";
constexpr char kEmail2[] = "user2@gmail.com";
constexpr char kAvatarUrl2[] = "https://google.com/avatar2.png";

GroupMember MakeGroupMember(const std::string& gaia_id,
                            const std::string& display_name,
                            const std::string& email,
                            MemberRole role,
                            const std::string& avatar_url) {
  GroupMember member;
  member.gaia_id = gaia_id;
  member.display_name = display_name;
  member.email = email;
  member.role = role;
  member.avatar_url = GURL(avatar_url);
  return member;
}

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

TEST(GroupDataProtoUtilsTest, ShouldConvertGroupDataToProto) {
  GroupData group_data;
  group_data.group_token.group_id = GroupId(kGroupId);
  group_data.display_name = kGroupDisplayName;
  group_data.members.push_back(MakeGroupMember(
      kGaiaId1, kUser1DisplayName, kEmail1, MemberRole::kOwner, kAvatarUrl1));
  group_data.group_token.access_token = kGroupAccessToken;

  data_sharing_pb::GroupData group_data_proto = GroupDataToProto(group_data);

  EXPECT_EQ(group_data_proto.group_id(), kGroupId);
  EXPECT_EQ(group_data_proto.display_name(), kGroupDisplayName);
  EXPECT_EQ(group_data_proto.access_token(), kGroupAccessToken);

  ASSERT_EQ(group_data_proto.members_size(), 1);

  const data_sharing_pb::GroupMember& member_proto =
      group_data_proto.members(0);
  EXPECT_EQ(member_proto.gaia_id(), kGaiaId1);
  EXPECT_EQ(member_proto.display_name(), kUser1DisplayName);
  EXPECT_EQ(member_proto.email(), kEmail1);
  EXPECT_EQ(member_proto.role(), data_sharing_pb::MEMBER_ROLE_OWNER);
  EXPECT_EQ(member_proto.avatar_url(), kAvatarUrl1);
}

TEST(GroupDataProtoUtilsTest, ShouldMakeGroupDataFromProto) {
  data_sharing_pb::GroupData group_data_proto;
  group_data_proto.set_group_id(kGroupId);
  group_data_proto.set_display_name(kGroupDisplayName);
  *group_data_proto.add_members() = MakeGroupMemberProto(
      kGaiaId1, kUser1DisplayName, kEmail1,
      data_sharing_pb::MemberRole::MEMBER_ROLE_OWNER, kAvatarUrl1);
  group_data_proto.set_access_token(kGroupAccessToken);

  GroupData group_data = GroupDataFromProto(group_data_proto);

  EXPECT_EQ(group_data.group_token.group_id, GroupId(kGroupId));
  EXPECT_EQ(group_data.display_name, kGroupDisplayName);
  EXPECT_EQ(group_data.group_token.access_token, kGroupAccessToken);

  ASSERT_THAT(group_data.members, SizeIs(1));
  const GroupMember& member = group_data.members[0];
  EXPECT_EQ(member.gaia_id, kGaiaId1);
  EXPECT_EQ(member.display_name, kUser1DisplayName);
  EXPECT_EQ(member.email, kEmail1);
  EXPECT_EQ(member.role, MemberRole::kOwner);
  EXPECT_EQ(member.avatar_url.spec(), kAvatarUrl1);
}

TEST(GroupDataProtoUtilsTest,
     ShouldConvertGroupDataToProtoAndBackWithMultipleMembers) {
  GroupData original_group_data;
  original_group_data.group_token.group_id = GroupId(kGroupId);
  original_group_data.display_name = kGroupDisplayName;
  original_group_data.members.push_back(MakeGroupMember(
      kGaiaId1, kUser1DisplayName, kEmail1, MemberRole::kOwner, kAvatarUrl1));
  original_group_data.members.push_back(MakeGroupMember(
      kGaiaId2, kUser2DisplayName, kEmail2, MemberRole::kMember, kAvatarUrl2));
  original_group_data.group_token.access_token = kGroupAccessToken;

  GroupData group_data_from_proto =
      GroupDataFromProto(GroupDataToProto(original_group_data));

  EXPECT_EQ(group_data_from_proto.group_token.group_id,
            original_group_data.group_token.group_id);
  EXPECT_EQ(group_data_from_proto.display_name, kGroupDisplayName);
  EXPECT_EQ(group_data_from_proto.group_token.access_token, kGroupAccessToken);

  ASSERT_THAT(group_data_from_proto.members, SizeIs(2));

  const GroupMember& member1 = group_data_from_proto.members[0];
  EXPECT_EQ(member1.gaia_id, kGaiaId1);
  EXPECT_EQ(member1.display_name, kUser1DisplayName);
  EXPECT_EQ(member1.email, kEmail1);
  EXPECT_EQ(member1.role, MemberRole::kOwner);
  EXPECT_EQ(member1.avatar_url.spec(), kAvatarUrl1);

  const GroupMember& member2 = group_data_from_proto.members[1];
  EXPECT_EQ(member2.gaia_id, kGaiaId2);
  EXPECT_EQ(member2.display_name, kUser2DisplayName);
  EXPECT_EQ(member2.email, kEmail2);
  EXPECT_EQ(member2.role, MemberRole::kMember);
  EXPECT_EQ(member2.avatar_url.spec(), kAvatarUrl2);
}

}  // namespace

}  // namespace data_sharing
