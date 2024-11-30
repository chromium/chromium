// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/collaboration_controller.h"
#include "components/collaboration/test_support/mock_collaboration_controller_delegate.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
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

  void SetUp() override {
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    InitService();
  }

  void TearDown() override { service_.reset(); }

  void InitService() {
    service_ = std::make_unique<CollaborationServiceImpl>(
        /*tab_group_sync_service=*/nullptr, &mock_data_sharing_service_,
        identity_test_env_.identity_manager(), test_sync_service_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
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

  // Invalid url parsing starts a join flow with empty GroupToken.
  service_->StartJoinFlow(
      std::make_unique<MockCollaborationControllerDelegate>(), url);
  const std::map<data_sharing::GroupToken,
                 std::unique_ptr<CollaborationController>>& join_flows =
      service_->GetJoinControllersForTesting();
  EXPECT_EQ(join_flows.size(), 1u);
  EXPECT_TRUE(join_flows.find(data_sharing::GroupToken()) != join_flows.end());

  // New join flow will be appended with a valid url parsing.
  EXPECT_CALL(mock_data_sharing_service_, ParseDataSharingUrl(url))
      .WillRepeatedly(Return(base::ok(token)));
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  EXPECT_CALL(*mock_delegate, PromoteCurrentScreen());
  service_->StartJoinFlow(std::move(mock_delegate), url);
  EXPECT_EQ(service_->GetJoinControllersForTesting().size(), 2u);

  // Existing join flow should not start a new flow and should promote the
  // existing flow's delegate.
  service_->StartJoinFlow(
      std::make_unique<MockCollaborationControllerDelegate>(), url);
  EXPECT_EQ(service_->GetJoinControllersForTesting().size(), 2u);
}

TEST_F(CollaborationServiceImplTest, SyncStatusChanges) {
  // By default the test sync service is signed in with sync and every DataType
  // enabled.
  EXPECT_EQ(service_->GetServiceStatus().sync_status, SyncStatus::kSyncEnabled);

  // Remove user's tab group setting.
  test_sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{});
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status,
            SyncStatus::kSyncWithoutTabGroup);

  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // If sync-the-feature is not required, kNotSyncing is never happening.
    test_sync_service_->SetSignedOut();
    test_sync_service_->FireStateChanged();
    EXPECT_EQ(service_->GetServiceStatus().sync_status,
              SyncStatus::kSyncWithoutTabGroup);
  } else {
    // Sign out removes sync consent.
    test_sync_service_->SetSignedOut();
    test_sync_service_->FireStateChanged();
    EXPECT_EQ(service_->GetServiceStatus().sync_status,
              SyncStatus::kNotSyncing);
  }
}

TEST_F(CollaborationServiceImplTest, SigninStatusChanges) {
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kNotSignedIn);

  identity_test_env_.SetPrimaryAccount(kUserEmail,
                                       signin::ConsentLevel::kSignin);
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kSignedInPaused);

  identity_test_env_.SetRefreshTokenForPrimaryAccount();
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kSignedIn);
}

}  // namespace collaboration
