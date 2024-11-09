// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using data_sharing::GroupData;
using data_sharing::GroupId;
using data_sharing::GroupMember;
using data_sharing::MemberRole;
using testing::Return;

namespace collaboration {

namespace {

const char kUserGaia[] = "gaia_id";
const char kUserEmail[] = "test@email.com";
const char kGroupId[] = "/?-group_id";
const char kAccessToken[] = "/?-access_token";

}  // namespace

class CollaborationServiceImplTest : public testing::Test {
 public:
  CollaborationServiceImplTest() = default;

  ~CollaborationServiceImplTest() override = default;

  void SetUp() override { InitService(); }

  void TearDown() override { service_.reset(); }

  void InitService() {
    service_ = std::make_unique<CollaborationServiceImpl>(
        /*tab_group_sync_service=*/nullptr, &mock_data_sharing_service_,
        identity_test_env_.identity_manager(),
        /*sync_service=*/nullptr);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  data_sharing::MockDataSharingService mock_data_sharing_service_;
  std::unique_ptr<CollaborationServiceImpl> service_;
};

TEST_F(CollaborationServiceImplTest, ConstructionAndEmptyServiceCheck) {
  EXPECT_FALSE(service_->IsEmptyService());
}

TEST_F(CollaborationServiceImplTest, GetCurrentUserRoleForGroup) {
  GroupData group_data = GroupData();
  GroupMember group_member = GroupMember();
  group_member.gaia_id = kUserGaia;
  group_member.role = MemberRole::kOwner;
  group_data.members.push_back(group_member);

  data_sharing::GroupId group_id = data_sharing::GroupId(kGroupId);

  // Empty or non existent group should return unknown role.
  EXPECT_CALL(mock_data_sharing_service_, ReadGroup(group_id))
      .WillOnce(Return(std::nullopt));

  EXPECT_EQ(service_->GetCurrentUserRoleForGroup(group_id),
            MemberRole::kUnknown);

  // No current primary account should return unknown role.
  EXPECT_CALL(mock_data_sharing_service_, ReadGroup(group_id))
      .WillRepeatedly(Return(group_data));
  EXPECT_EQ(service_->GetCurrentUserRoleForGroup(group_id),
            MemberRole::kUnknown);

  identity_test_env_.MakeAccountAvailable(
      kUserEmail,
      {.primary_account_consent_level = signin::ConsentLevel::kSignin,
       .gaia_id = kUserGaia});
  EXPECT_EQ(service_->GetCurrentUserRoleForGroup(group_id), MemberRole::kOwner);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_Disabled) {
  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kDisabled);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_JoinOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      data_sharing::features::kDataSharingJoinOnly);
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kAllowedToJoin);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_Create) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      data_sharing::features::kDataSharingFeature);
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kEnabledCreateAndJoin);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_CreateOverridesJoinOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({data_sharing::features::kDataSharingJoinOnly,
                                 data_sharing::features::kDataSharingFeature},
                                {});
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kEnabledCreateAndJoin);
}

TEST_F(CollaborationServiceImplTest, StartJoinFlow) {
  GURL url("http://www.example.com/");
  data_sharing::GroupToken token(data_sharing::GroupId(kGroupId), kAccessToken);

  EXPECT_CALL(mock_data_sharing_service_, ParseDataSharingUrl(url))
      .WillOnce(Return(
          base::unexpected(data_sharing::MockDataSharingService::
                               ParseUrlStatus::kHostOrPathMismatchFailure)));

  // Invalid url parsing will not start new join flow.
  service_->StartJoinFlow(/*delegate=*/nullptr, url);
  EXPECT_EQ(service_->GetJoinControllersForTesting().size(), 0u);

  EXPECT_CALL(mock_data_sharing_service_, ParseDataSharingUrl(url))
      .WillRepeatedly(Return(base::ok(token)));

  // New join flow will be appended.
  service_->StartJoinFlow(/*delegate=*/nullptr, url);
  EXPECT_EQ(service_->GetJoinControllersForTesting().size(), 1u);

  // Existing join flow should not start new flow.
  service_->StartJoinFlow(/*delegate=*/nullptr, url);
  EXPECT_EQ(service_->GetJoinControllersForTesting().size(), 1u);
}

}  // namespace collaboration
