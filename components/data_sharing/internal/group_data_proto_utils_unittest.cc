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

constexpr GaiaId::Literal kGaiaId3("gaia-id3");
constexpr char kUser3DisplayName[] = "user3-display-name";
constexpr char kEmail3[] = "user3@gmail.com";
constexpr char kAvatarUrl3[] = "https://google.com/avatar3.png";

constexpr GaiaId::Literal kGaiaId4("gaia-id4");
constexpr char kUser4DisplayName[] = "user4-display-name";
constexpr char kEmail4[] = "user4@gmail.com";
constexpr char kAvatarUrl4[] = "https://google.com/avatar4.png";

constexpr GaiaId::Literal kGaiaId5("gaia-id5");
constexpr char kUser5DisplayName[] = "user5-display-name";
constexpr char kEmail5[] = "user5@gmail.com";
constexpr char kAvatarUrl5[] = "https://google.com/avatar5.png";

constexpr GaiaId::Literal kGaiaId6("gaia-id6");
constexpr char kUser6DisplayName[] = "user6-display-name";
constexpr char kEmail6[] = "user6@gmail.com";
constexpr char kAvatarUrl6[] = "https://google.com/avatar6.png";

const int kCreationTimeUnixEpocMillis1 = 1;
const int kCreationTimeUnixEpocMillis2 = 2;
const int kLastUpdatedTimeUnixEpochMillis1 = 3;
const int kLastUpdatedTimeUnixEpochMillis2 = 4;

data_sharing_pb::GroupMember MakeGroupMemberProto(
    const std::string& gaia_id,
    const std::string& display_name,
    const std::string& email,
    data_sharing_pb::MemberRole role,
    const std::string& avatar_url,
    int creation_time_unix_epoch_millis,
    int last_updated_time_unix_epoch_millis) {
  data_sharing_pb::GroupMember member_proto;
  member_proto.set_gaia_id(gaia_id);
  member_proto.set_display_name(display_name);
  member_proto.set_email(email);
  member_proto.set_role(role);
  member_proto.set_avatar_url(avatar_url);
  member_proto.set_creation_time_unix_epoch_millis(
      creation_time_unix_epoch_millis);
  member_proto.set_last_updated_time_unix_epoch_millis(
      last_updated_time_unix_epoch_millis);
  return member_proto;
}

TEST(GroupDataProtoUtilsTest, ShouldMakeGroupDataFromProto) {
  data_sharing_pb::GroupData group_data_proto;
  group_data_proto.set_group_id(kGroupId);
  group_data_proto.set_display_name(kGroupDisplayName);
  *group_data_proto.add_members() = MakeGroupMemberProto(
      kGaiaId1.ToString(), kUser1DisplayName, kEmail1,
      data_sharing_pb::MemberRole::MEMBER_ROLE_OWNER, kAvatarUrl1,
      kCreationTimeUnixEpocMillis1, kLastUpdatedTimeUnixEpochMillis1);
  *group_data_proto.add_former_members() = MakeGroupMemberProto(
      kGaiaId2.ToString(), kUser2DisplayName, kEmail2,
      data_sharing_pb::MemberRole::MEMBER_ROLE_FORMER_MEMBER, kAvatarUrl2,
      kCreationTimeUnixEpocMillis2, kLastUpdatedTimeUnixEpochMillis2);

  // Add members of other types. These should be ignored.
  *group_data_proto.add_members() = MakeGroupMemberProto(
      kGaiaId3.ToString(), kUser3DisplayName, kEmail3,
      data_sharing_pb::MemberRole::MEMBER_ROLE_INVITEE, kAvatarUrl3, 5, 6);
  *group_data_proto.add_members() = MakeGroupMemberProto(
      kGaiaId4.ToString(), kUser4DisplayName, kEmail4,
      data_sharing_pb::MemberRole::MEMBER_ROLE_UNSPECIFIED, kAvatarUrl4, 7, 8);
  *group_data_proto.add_members() = MakeGroupMemberProto(
      kGaiaId5.ToString(), kUser5DisplayName, kEmail5,
      data_sharing_pb::MemberRole::MEMBER_ROLE_FORMER_MEMBER, kAvatarUrl5, 1,
      3);

  // Add former members of wrong type. This should be ignored.
  *group_data_proto.add_former_members() = MakeGroupMemberProto(
      kGaiaId6.ToString(), kUser6DisplayName, kEmail6,
      data_sharing_pb::MemberRole::MEMBER_ROLE_MEMBER, kAvatarUrl6, 2, 4);

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
  EXPECT_EQ(member.creation_time, base::Time::FromMillisecondsSinceUnixEpoch(
                                      kCreationTimeUnixEpocMillis1));
  EXPECT_EQ(member.last_updated_time,
            base::Time::FromMillisecondsSinceUnixEpoch(
                kLastUpdatedTimeUnixEpochMillis1));

  ASSERT_THAT(group_data.former_members, SizeIs(1));
  const GroupMember& former_member = group_data.former_members[0];
  EXPECT_EQ(former_member.gaia_id, kGaiaId2);
  EXPECT_EQ(former_member.display_name, kUser2DisplayName);
  EXPECT_EQ(former_member.email, kEmail2);
  EXPECT_EQ(former_member.role, MemberRole::kFormerMember);
  EXPECT_EQ(former_member.avatar_url.spec(), kAvatarUrl2);
  EXPECT_EQ(
      former_member.creation_time,
      base::Time::FromMillisecondsSinceUnixEpoch(kCreationTimeUnixEpocMillis2));
  EXPECT_EQ(former_member.last_updated_time,
            base::Time::FromMillisecondsSinceUnixEpoch(
                kLastUpdatedTimeUnixEpochMillis2));
}

}  // namespace

}  // namespace data_sharing
