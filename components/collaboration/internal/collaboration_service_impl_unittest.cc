// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_service_impl.h"

#include <memory>

#include "base/android/device_info.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/collaboration/internal/collaboration_controller.h"
#include "components/collaboration/public/pref_names.h"
#include "components/collaboration/test_support/mock_collaboration_controller_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/test_support/mock_data_sharing_service.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using data_sharing::GroupData;
using data_sharing::GroupId;
using data_sharing::GroupMember;
using data_sharing::MemberRole;
using signin::PrimaryAccountChangeEvent;
using testing::_;
using testing::Return;

namespace collaboration {

namespace {

constexpr GaiaId::Literal kUserGaia("gaia_id");
constexpr GaiaId::Literal kOtherUserGaia("other_gaia_id");
constexpr char kConsumerUserEmail[] = "test@email.com";
constexpr char kOtherConsumerUserEmail[] = "other.test@email.com";
constexpr char kManagedUserEmail[] = "test@google.com";
constexpr char kGroupId[] = "/?-group_id";
constexpr char kAccessToken[] = "/?-access_token";

}  // namespace

class CollaborationServiceImplTest : public testing::Test {
 public:
  CollaborationServiceImplTest() = default;

  ~CollaborationServiceImplTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::device_info::is_automotive()) {
      // TODO(crbug.com/399444939): Re-enable once automotive is supported.
      GTEST_SKIP() << "Test shouldn't run on automotive builders.";
    }
#endif
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    profile_pref_service_.registry()->RegisterIntegerPref(
        prefs::kSharedTabGroupsManagedAccountSetting, 0 /* enabled */);
    profile_pref_service_.registry()->RegisterBooleanPref(
        ::prefs::kSigninAllowed, true);
    profile_pref_service_.registry()->RegisterBooleanPref(
        ::prefs::kSigninAllowedOnNextStartup, true);
#if BUILDFLAG(IS_IOS)
    local_pref_service_.registry()->RegisterIntegerPref(
        ::prefs::kBrowserSigninPolicy,
        static_cast<int>(BrowserSigninMode::kEnabled));
#endif
    InitService();
  }

  void TearDown() override { service_.reset(); }

  void InitService() {
    service_ = std::make_unique<CollaborationServiceImpl>(
        &mock_tab_group_sync_service_, &mock_data_sharing_service_,
        identity_test_env_.identity_manager(), &profile_pref_service_,
        &local_pref_service_);
    service_->OnSyncServiceInitialized(test_sync_service_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable profile_pref_service_;
  sync_preferences::TestingPrefServiceSyncable local_pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  tab_groups::MockTabGroupSyncService mock_tab_group_sync_service_;
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
      kConsumerUserEmail,
      {.primary_account_consent_level = signin::ConsentLevel::kSignin,
       .gaia_id = kUserGaia});
  EXPECT_EQ(service_->GetCurrentUserRoleForGroup(group_id), MemberRole::kOwner);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {data_sharing::features::kDataSharingFeature,
                                 data_sharing::features::kDataSharingJoinOnly});
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kDisabled);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_JoinOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({data_sharing::features::kDataSharingJoinOnly},
                                {data_sharing::features::kDataSharingFeature});
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kAllowedToJoin);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_Create) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                                {data_sharing::features::kDataSharingJoinOnly});
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

TEST_F(CollaborationServiceImplTest, GetServiceStatus_SigninDisabledByUser) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      data_sharing::features::kDataSharingFeature);

  // Set signin preference to disable signin.
  profile_pref_service_.SetBoolean(::prefs::kSigninAllowed, false);
  profile_pref_service_.SetBoolean(::prefs::kSigninAllowedOnNextStartup, false);

  InitService();

  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kSigninDisabled);
  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kEnabledCreateAndJoin);
  EXPECT_EQ(service_->GetServiceStatus().IsAllowedToJoin(), true);
  EXPECT_EQ(service_->GetServiceStatus().IsAllowedToCreate(), false);
}

#if !BUILDFLAG(IS_IOS)
TEST_F(CollaborationServiceImplTest, GetServiceStatus_SigninDisabledByPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      data_sharing::features::kDataSharingFeature);

#if BUILDFLAG(IS_ANDROID)
  profile_pref_service_.SetBoolean(::prefs::kSigninAllowed, false);
  profile_pref_service_.SetManagedPref(::prefs::kSigninAllowed,
                                       base::Value(false));
#else
  profile_pref_service_.SetBoolean(::prefs::kSigninAllowedOnNextStartup, false);
  profile_pref_service_.SetManagedPref(::prefs::kSigninAllowedOnNextStartup,
                                       base::Value(false));
#endif

  InitService();
  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kDisabledForPolicy);
}
#endif

TEST_F(CollaborationServiceImplTest, GetServiceStatus_ManagedAccount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      data_sharing::features::kDataSharingFeature);
  InitService();
  profile_pref_service_.SetInteger(prefs::kSharedTabGroupsManagedAccountSetting,
                                   1 /* disabled */);

  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kNotSignedIn);
  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kEnabledCreateAndJoin);

  // Signin a managed account.
  identity_test_env_.MakePrimaryAccountAvailable(kManagedUserEmail,
                                                 signin::ConsentLevel::kSignin);
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kSignedIn);
  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kDisabledForPolicy);

  // Signin a consumer account re-enable the feature.
  identity_test_env_.MakePrimaryAccountAvailable(kConsumerUserEmail,
                                                 signin::ConsentLevel::kSignin);
  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kEnabledCreateAndJoin);
}

TEST_F(CollaborationServiceImplTest, StartJoinFlow) {
  GURL url("http://www.example.com/");

  // Invalid url parsing starts a join flow with empty GroupToken.
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate_invalid =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_invalid_ptr =
      mock_delegate_invalid.get();
  EXPECT_CALL(*mock_delegate_invalid, OnFlowFinished());
  service_->StartJoinFlow(std::move(mock_delegate_invalid), url);
  // Wait for post tasks.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetJoinControllersForTesting().size() == 1; }));
  const std::map<data_sharing::GroupToken,
                 std::unique_ptr<CollaborationController>>& join_flows =
      service_->GetJoinControllersForTesting();
  EXPECT_TRUE(join_flows.find(data_sharing::GroupToken()) != join_flows.end());

  // New join flow will be appended with a valid url parsing and will stop all
  // conflicting flows.
  url = GURL(data_sharing::features::kDataSharingURL.Get() + "?g=" + kGroupId +
             "&t=" + kAccessToken);
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  EXPECT_CALL(*mock_delegate, OnFlowFinished());

  bool invalid_cancel_called = false;
  EXPECT_CALL(*delegate_invalid_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        invalid_cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });

  service_->StartJoinFlow(std::move(mock_delegate), url);

  // Wait for post tasks.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetJoinControllersForTesting().size() == 1; }));

  EXPECT_TRUE(invalid_cancel_called);

  bool cancel_called = false;
  EXPECT_CALL(*delegate_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });

  // Existing join flow will stop all conflicting flows and will be appended
  // similar to a new join flow.
  service_->StartJoinFlow(
      std::make_unique<MockCollaborationControllerDelegate>(), url);
  EXPECT_EQ(service_->GetJoinControllersForTesting().size(), 1u);
  EXPECT_TRUE(cancel_called);
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

TEST_F(CollaborationServiceImplTest, SyncTypeDisabledByEnterprise) {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // Set up a policy to disable Tabs.
  test_sync_service_->GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kTabs, true);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status,
            SyncStatus::kSyncDisabledByEnterprise);

  // Reset the policy.
  test_sync_service_->GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kTabs, false);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status, SyncStatus::kSyncEnabled);

  // Set up a policy to disable History.
  test_sync_service_->GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kHistory, true);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status,
            SyncStatus::kSyncDisabledByEnterprise);
#else
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  // Set up a policy to disable Saved Tab Groups.
  test_sync_service_->GetUserSettings()->SetTypeIsManagedByPolicy(
      syncer::UserSelectableType::kSavedTabGroups, true);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status,
            SyncStatus::kSyncDisabledByEnterprise);
#endif
}

TEST_F(CollaborationServiceImplTest, SyncDisabledByEnterprise) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  // Disable sync by enterprise policy.
  test_sync_service_->SetAllowedByEnterprisePolicy(false);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status,
            SyncStatus::kSyncDisabledByEnterprise);
}

TEST_F(CollaborationServiceImplTest, SyncStatusChanges_SettingInProgress) {
  // By default the test sync service is signed in with sync and every DataType
  // enabled.
  EXPECT_EQ(service_->GetServiceStatus().sync_status, SyncStatus::kSyncEnabled);

  // Setup in progress does not change sync status.
  test_sync_service_->SetSetupInProgress();
  test_sync_service_->SetSignedOut();
  test_sync_service_->FireStateChanged();
  EXPECT_EQ(service_->GetServiceStatus().sync_status, SyncStatus::kSyncEnabled);
}

TEST_F(CollaborationServiceImplTest, ConsumerSigninChanges) {
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kNotSignedIn);

  identity_test_env_.SetPrimaryAccount(kConsumerUserEmail,
                                       signin::ConsentLevel::kSignin);
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kSignedInPaused);

  identity_test_env_.SetRefreshTokenForPrimaryAccount();
  EXPECT_EQ(service_->GetServiceStatus().signin_status,
            SigninStatus::kSignedIn);
}

TEST_F(CollaborationServiceImplTest, DeleteGroup) {
  data_sharing::GroupId group_id = data_sharing::GroupId(kGroupId);
  EXPECT_CALL(mock_tab_group_sync_service_,
              OnCollaborationRemoved(syncer::CollaborationId(kGroupId)));
  EXPECT_CALL(mock_data_sharing_service_, DeleteGroup(group_id, _))
      .WillOnce(
          [](const data_sharing::GroupId&,
             base::OnceCallback<void(
                 data_sharing::DataSharingService::PeopleGroupActionOutcome)>
                 callback) {
            std::move(callback).Run(data_sharing::DataSharingService::
                                        PeopleGroupActionOutcome::kSuccess);
          });

  base::RunLoop run_loop;
  service_->DeleteGroup(group_id,
                        base::BindOnce(
                            [](base::RunLoop* run_loop, bool success) {
                              ASSERT_TRUE(success);
                              run_loop->Quit();
                            },
                            &run_loop));
  run_loop.Run();
}

TEST_F(CollaborationServiceImplTest, LeaveGroup) {
  data_sharing::GroupId group_id = data_sharing::GroupId(kGroupId);
  EXPECT_CALL(mock_tab_group_sync_service_,
              OnCollaborationRemoved(syncer::CollaborationId(kGroupId)));
  EXPECT_CALL(mock_data_sharing_service_, LeaveGroup(group_id, _))
      .WillOnce(
          [](const data_sharing::GroupId&,
             base::OnceCallback<void(
                 data_sharing::DataSharingService::PeopleGroupActionOutcome)>
                 callback) {
            std::move(callback).Run(data_sharing::DataSharingService::
                                        PeopleGroupActionOutcome::kSuccess);
          });

  base::RunLoop run_loop;
  service_->LeaveGroup(group_id, base::BindOnce(
                                     [](base::RunLoop* run_loop, bool success) {
                                       ASSERT_TRUE(success);
                                       run_loop->Quit();
                                     },
                                     &run_loop));
  run_loop.Run();
}

TEST_F(CollaborationServiceImplTest, CancelAllFlows) {
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kGroupId + "&t=" + kAccessToken);

  // New join flow will be appended with a valid url parsing and will stop all
  // conflicting flows.
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  EXPECT_CALL(*mock_delegate, OnFlowFinished());
  service_->StartJoinFlow(std::move(mock_delegate), url);

  // Wait for post tasks.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetJoinControllersForTesting().size() == 1; }));

  bool cancel_called = false;
  EXPECT_CALL(*delegate_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });
  service_->CancelAllFlows();

  EXPECT_TRUE(cancel_called);

  // Wait for post tasks.
  EXPECT_TRUE(service_->GetJoinControllersForTesting().size() == 0);
  EXPECT_TRUE(service_->GetDeletingControllersCountForTesting() == 1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service_->GetDeletingControllersCountForTesting() == 0;
  }));
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_NoChange_DoesntCancelJoin) {
  // Start a join flow.
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kGroupId + "&t=" + kAccessToken);
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartJoinFlow(std::move(mock_delegate), url);
  EXPECT_CALL(*delegate_ptr, Cancel(_)).Times(0);
  // Prepare a kNoChange event.
  PrimaryAccountChangeEvent::State state;
  PrimaryAccountChangeEvent event_details(
      state, state, signin_metrics::AccessPoint::kUnknown);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_NoChange_DoesntCancelShare) {
  // Start a share flow.
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartShareOrManageFlow(
      std::move(mock_delegate), tab_groups::test::GenerateRandomTabGroupID(),
      CollaborationServiceShareOrManageEntryPoint::kUnknown);
  EXPECT_CALL(*delegate_ptr, Cancel(_)).Times(0);
  // Prepare a kNoChange event.
  PrimaryAccountChangeEvent::State state;
  PrimaryAccountChangeEvent event_details(
      state, state, signin_metrics::AccessPoint::kUnknown);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_SigningInFromUnsignedIn_DoesntCancelJoin) {
  // Start a join flow.
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kGroupId + "&t=" + kAccessToken);
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartJoinFlow(std::move(mock_delegate), url);
  EXPECT_CALL(*delegate_ptr, Cancel(_)).Times(0);
  // Prepare a kSet event from unsigned in to signed in.
  PrimaryAccountChangeEvent::State previous_state;
  CoreAccountInfo account_info;
  account_info.gaia = kUserGaia;
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  account_info.email = kConsumerUserEmail;
  PrimaryAccountChangeEvent::State current_state(account_info,
                                                 signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::AccessPoint::kUnknown);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_SigningInFromUnsignedIn_DoesntCancelShare) {
  // Start a share flow.
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartShareOrManageFlow(
      std::move(mock_delegate), tab_groups::test::GenerateRandomTabGroupID(),
      CollaborationServiceShareOrManageEntryPoint::kUnknown);
  EXPECT_CALL(*delegate_ptr, Cancel(_)).Times(0);
  // Prepare a kSet event from unsigned in to signed in.
  PrimaryAccountChangeEvent::State previous_state;
  CoreAccountInfo account_info;
  account_info.gaia = kUserGaia;
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  account_info.email = kConsumerUserEmail;
  PrimaryAccountChangeEvent::State current_state(account_info,
                                                 signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::AccessPoint::kUnknown);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_SwitchingAccount_CancelsJoin) {
  // Start a join flow.
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kGroupId + "&t=" + kAccessToken);
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartJoinFlow(std::move(mock_delegate), url);
  bool cancel_called = false;
  EXPECT_CALL(*delegate_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });
  // Prepare a kSet event, switching accounts.
  CoreAccountInfo account_info_1;
  account_info_1.gaia = kUserGaia;
  account_info_1.account_id = CoreAccountId::FromGaiaId(account_info_1.gaia);
  account_info_1.email = kConsumerUserEmail;
  PrimaryAccountChangeEvent::State previous_state(
      account_info_1, signin::ConsentLevel::kSignin);
  CoreAccountInfo account_info_2;
  account_info_2.gaia = kOtherUserGaia;
  account_info_2.account_id = CoreAccountId::FromGaiaId(account_info_2.gaia);
  account_info_2.email = kOtherConsumerUserEmail;
  PrimaryAccountChangeEvent::State current_state(account_info_2,
                                                 signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::AccessPoint::kUnknown);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);

  EXPECT_TRUE(cancel_called);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_SwitchingAccount_CancelsShare) {
  // Start a share flow.
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartShareOrManageFlow(
      std::move(mock_delegate), tab_groups::test::GenerateRandomTabGroupID(),
      CollaborationServiceShareOrManageEntryPoint::kUnknown);
  bool cancel_called = false;
  EXPECT_CALL(*delegate_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });
  // Prepare a kSet event, switching accounts.
  CoreAccountInfo account_info_1;
  account_info_1.gaia = kUserGaia;
  account_info_1.account_id = CoreAccountId::FromGaiaId(account_info_1.gaia);
  account_info_1.email = kConsumerUserEmail;
  PrimaryAccountChangeEvent::State previous_state(
      account_info_1, signin::ConsentLevel::kSignin);
  CoreAccountInfo account_info_2;
  account_info_2.gaia = kOtherUserGaia;
  account_info_2.account_id = CoreAccountId::FromGaiaId(account_info_2.gaia);
  account_info_2.email = kOtherConsumerUserEmail;
  PrimaryAccountChangeEvent::State current_state(account_info_2,
                                                 signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::AccessPoint::kUnknown);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);

  EXPECT_TRUE(cancel_called);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_Cleared_CancelsJoin) {
  // Start a join flow.
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kGroupId + "&t=" + kAccessToken);
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartJoinFlow(std::move(mock_delegate), url);
  bool cancel_called = false;
  EXPECT_CALL(*delegate_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });
  // Prepare a kCleared event.
  CoreAccountInfo account_info;
  account_info.gaia = kUserGaia;
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  account_info.email = kConsumerUserEmail;
  PrimaryAccountChangeEvent::State previous_state(
      account_info, signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent::State current_state;
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::ProfileSignout::kTest);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);

  EXPECT_TRUE(cancel_called);
}

TEST_F(CollaborationServiceImplTest,
       OnPrimaryAccountChanged_Cleared_CancelsShare) {
  // Start a share flow.
  std::unique_ptr<MockCollaborationControllerDelegate> mock_delegate =
      std::make_unique<MockCollaborationControllerDelegate>();
  MockCollaborationControllerDelegate* delegate_ptr = mock_delegate.get();
  service_->StartShareOrManageFlow(
      std::move(mock_delegate), tab_groups::test::GenerateRandomTabGroupID(),
      CollaborationServiceShareOrManageEntryPoint::kUnknown);
  bool cancel_called = false;
  EXPECT_CALL(*delegate_ptr, Cancel(_))
      .WillOnce([&](CollaborationControllerDelegate::ResultCallback result) {
        cancel_called = true;
        std::move(result).Run(
            CollaborationControllerDelegate::Outcome::kSuccess);
        return true;
      });
  // Prepare a kCleared event.
  CoreAccountInfo account_info;
  account_info.gaia = kUserGaia;
  account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
  account_info.email = kConsumerUserEmail;
  PrimaryAccountChangeEvent::State previous_state(
      account_info, signin::ConsentLevel::kSignin);
  PrimaryAccountChangeEvent::State current_state;
  PrimaryAccountChangeEvent event_details(
      previous_state, current_state, signin_metrics::ProfileSignout::kTest);

  // Process the event.
  service_->OnPrimaryAccountChanged(event_details);

  EXPECT_TRUE(cancel_called);
}

TEST_F(CollaborationServiceImplTest,
       GetServiceStatus_VersionOutOfDateShowUpdateChromeUi) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {data_sharing::features::kDataSharingFeature,
       data_sharing::features::kDataSharingEnableUpdateChromeUI,
       data_sharing::features::kSharedDataTypesKillSwitch},
      {data_sharing::features::kDataSharingJoinOnly});
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kVersionOutOfDateShowUpdateChromeUi);
}

TEST_F(CollaborationServiceImplTest, GetServiceStatus_VersionOutOfDate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          data_sharing::features::kDataSharingJoinOnly,
          data_sharing::features::kSharedDataTypesKillSwitch,
      },
      {data_sharing::features::kDataSharingFeature,
       data_sharing::features::kDataSharingEnableUpdateChromeUI});
  InitService();

  EXPECT_EQ(service_->GetServiceStatus().collaboration_status,
            CollaborationStatus::kVersionOutOfDate);
}

}  // namespace collaboration
