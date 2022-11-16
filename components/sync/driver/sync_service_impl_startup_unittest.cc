// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "components/sync/driver/sync_service_impl.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "components/sync/test/fake_sync_api_component_factory.h"
#include "components/sync/test/fake_sync_engine.h"
#include "components/sync/test/sync_client_mock.h"
#include "components/sync/test/sync_service_impl_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace syncer {

namespace {

const char kEmail[] = "test_user@gmail.com";

class MockSyncServiceObserver : public SyncServiceObserver {
 public:
  MockSyncServiceObserver() = default;

  MOCK_METHOD(void, OnStateChanged, (SyncService*), (override));
};

}  // namespace

class SyncServiceImplStartupTest : public testing::Test {
 public:
  SyncServiceImplStartupTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        sync_prefs_(sync_service_impl_bundle_.pref_service()) {
    sync_service_impl_bundle_.identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  ~SyncServiceImplStartupTest() override { sync_service_->Shutdown(); }

  void CreateSyncService(
      SyncServiceImpl::StartBehavior start_behavior,
      ModelTypeSet registered_types = ModelTypeSet(BOOKMARKS)) {
    DataTypeController::TypeVector controllers;
    for (ModelType type : registered_types) {
      auto controller = std::make_unique<FakeDataTypeController>(type);
      // Hold a raw pointer to directly interact with the controller.
      controller_map_[type] = controller.get();
      controllers.push_back(std::move(controller));
    }

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    sync_service_ = std::make_unique<SyncServiceImpl>(
        sync_service_impl_bundle_.CreateBasicInitParams(
            start_behavior, std::move(sync_client)));
  }

  void SimulateTestUserSignin() {
    sync_service_impl_bundle_.identity_test_env()->MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSync);
  }

  void SimulateRefreshTokensNotLoadedYet() {
    // First, wait for the actual refresh token load to complete if necessary.
    // Otherwise, if it was still ongoing, it might reset the state back to
    // "everything loaded" once it completes.
    sync_service_impl_bundle_.identity_test_env()->WaitForRefreshTokensLoaded();
    sync_service_impl_bundle_.identity_test_env()
        ->ResetToAccountsNotYetLoadedFromDiskState();
  }

  void SimulateRefreshTokensLoad() {
    sync_service_impl_bundle_.identity_test_env()->ReloadAccountsFromDisk();
    sync_service_impl_bundle_.identity_test_env()->WaitForRefreshTokensLoaded();
  }

  void SimulateTestUserSigninWithoutRefreshToken() {
    // Set the primary account *without* providing an OAuth token.
    sync_service_impl_bundle_.identity_test_env()->SetPrimaryAccount(
        kEmail, signin::ConsentLevel::kSync);
  }

  void UpdateCredentials() {
    sync_service_impl_bundle_.identity_test_env()
        ->SetRefreshTokenForPrimaryAccount();
  }

  // Sets a special invalid refresh token. This is what happens when the primary
  // (and sync-consented) account signs out on the web.
  void SimulateWebSignout() {
    sync_service_impl_bundle_.identity_test_env()
        ->SetInvalidRefreshTokenForPrimaryAccount();
  }

  void DisableAutomaticIssueOfAccessTokens() {
    sync_service_impl_bundle_.identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(false);
  }

  void RespondToTokenRequest() {
    sync_service_impl_bundle_.identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            "access_token", base::Time::Max());
  }

  SyncPrefs* sync_prefs() { return &sync_prefs_; }

  SyncServiceImpl* sync_service() { return sync_service_.get(); }

  PrefService* pref_service() {
    return sync_service_impl_bundle_.pref_service();
  }

  FakeSyncApiComponentFactory* component_factory() {
    return sync_service_impl_bundle_.component_factory();
  }

  DataTypeManagerImpl* data_type_manager() {
    return component_factory()->last_created_data_type_manager();
  }

  FakeSyncEngine* engine() {
    return component_factory()->last_created_engine();
  }

  FakeDataTypeController* get_controller(ModelType type) {
    return controller_map_[type];
  }

  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  SyncServiceImplBundle sync_service_impl_bundle_;
  SyncPrefs sync_prefs_;
  std::unique_ptr<SyncServiceImpl> sync_service_;
  // The controllers are owned by |sync_service_|.
  std::map<ModelType, FakeDataTypeController*> controller_map_;
};

// ChromeOS does not support sign-in after startup
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplStartupTest, StartFirstTime) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  CreateSyncService(SyncServiceImpl::MANUAL_START);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  sync_service()->Initialize();
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
  EXPECT_EQ(nullptr, data_type_manager());
  EXPECT_FALSE(engine());

  // Preferences should be back to defaults.
  EXPECT_EQ(base::Time(), sync_service()->GetLastSyncedTimeForDebugging());
  EXPECT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // This tells the SyncServiceImpl that setup is now in progress, which
  // causes it to try starting up the engine. We're not signed in yet though, so
  // that won't work.
  sync_service()->GetUserSettings()->SetSyncRequested(true);
  std::unique_ptr<SyncSetupInProgressHandle> sync_blocker =
      sync_service()->GetSetupInProgressHandle();
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  SimulateTestUserSignin();
  base::RunLoop().RunUntilIdle();

  // Now we're signed in, so the engine can start. Engine initialization is
  // immediate in this test, so we bypass the INITIALIZING state.
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::DisableReasonSet(),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());

  // Simulate the UI telling sync it has finished setting up. Note that this is
  // a two-step process: Releasing the SetupInProgressHandle, and marking first
  // setup complete.
  // Since standalone transport is enabled, completed first-time setup is not a
  // requirement, so the service will start up as soon as the setup handle is
  // released.
  sync_blocker.reset();
  ASSERT_FALSE(sync_service()->IsSetupInProgress());
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still not active, but rather pending confirmation.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());

  // Marking first setup complete will let SyncServiceImpl reconfigure the
  // DataTypeManager in full Sync-the-feature mode.
  sync_service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());

  // This should have fully enabled sync.
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplStartupTest, StartNoCredentials) {
  // We're already signed in, but don't have a refresh token.
  SimulateRefreshTokensNotLoadedYet();
  SimulateTestUserSigninWithoutRefreshToken();
  sync_prefs()->SetFirstSetupComplete();

  CreateSyncService(SyncServiceImpl::MANUAL_START);
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();

  // SyncServiceImpl should now be active, but of course not have an access
  // token.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->GetAccessTokenForTest().empty());
  // Note that SyncServiceImpl is not in an auth error state - no auth was
  // attempted, so no error.
}

TEST_F(SyncServiceImplStartupTest, WebSignoutBeforeInitialization) {
  // There is a primary account, but it's in a "web signout" aka sync-paused
  // state.
  SimulateTestUserSignin();
  SimulateWebSignout();
  sync_prefs()->SetFirstSetupComplete();

  CreateSyncService(SyncServiceImpl::MANUAL_START);

  sync_service()->Initialize();

  // SyncServiceImpl should now be in the paused state.
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());
}

TEST_F(SyncServiceImplStartupTest, WebSignoutDuringDeferredStartup) {
  // There is a primary account. It is theoretically in the "web signout" aka
  // sync-paused error state, but the identity code hasn't detected that yet
  // (because auth errors are not persisted).
  SimulateTestUserSignin();
  sync_prefs()->SetFirstSetupComplete();

  // Note: Deferred startup is only enabled if SESSIONS is among the preferred
  // data types.
  CreateSyncService(SyncServiceImpl::MANUAL_START, {TYPED_URLS, SESSIONS});
  sync_service()->Initialize();

  ASSERT_EQ(SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());

  MockSyncServiceObserver observer;
  sync_service()->AddObserver(&observer);

  // Entering the sync-paused state should trigger a notification.
  // Note: Depending on the exact sequence of IdentityManager::Observer calls
  // (refresh token changed and/or auth error changed), there might be multiple
  // notifications.
  EXPECT_CALL(observer, OnStateChanged(sync_service()))
      .Times(testing::AtLeast(1))
      .WillRepeatedly([&]() {
        EXPECT_EQ(SyncService::TransportState::PAUSED,
                  sync_service()->GetTransportState());
      });

  // Now sign out on the web to enter the sync-paused state.
  SimulateWebSignout();

  // SyncServiceImpl should now be in the paused state.
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());

  sync_service()->RemoveObserver(&observer);
}

TEST_F(SyncServiceImplStartupTest, WebSignoutAfterInitialization) {
  // This test has to wait for the access token request to complete, so disable
  // automatic issuing of tokens.
  DisableAutomaticIssueOfAccessTokens();

  SimulateTestUserSignin();
  sync_prefs()->SetFirstSetupComplete();

  CreateSyncService(SyncServiceImpl::MANUAL_START);
  sync_service()->Initialize();

  // Respond to the token request to finish the initialization flow.
  RespondToTokenRequest();

  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  MockSyncServiceObserver observer;
  sync_service()->AddObserver(&observer);

  // Entering the sync-paused state should trigger a notification.
  // Note: Depending on the exact sequence of IdentityManager::Observer calls
  // (refresh token changed and/or auth error changed), there might be multiple
  // notifications.
  EXPECT_CALL(observer, OnStateChanged(sync_service()))
      .Times(testing::AtLeast(1))
      .WillRepeatedly([&]() {
        EXPECT_EQ(SyncService::TransportState::PAUSED,
                  sync_service()->GetTransportState());
      });

  // Now sign out on the web to enter the sync-paused state.
  SimulateWebSignout();

  // SyncServiceImpl should now be in the paused state.
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());

  sync_service()->RemoveObserver(&observer);
}

TEST_F(SyncServiceImplStartupTest, StartInvalidCredentials) {
  SimulateTestUserSignin();
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();

  CreateSyncService(SyncServiceImpl::MANUAL_START);

  // Prevent automatic (and successful) completion of engine initialization.
  component_factory()->AllowFakeEngineInitCompletion(false);
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  // Simulate an auth error while downloading control types.
  engine()->TriggerInitializationCompletion(/*success=*/false);

  // Engine initialization failures puts the service into an unrecoverable error
  // state. It'll take either a browser restart or a full sign-out+sign-in to
  // get out of this.
  EXPECT_TRUE(sync_service()->HasUnrecoverableError());
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
}

TEST_F(SyncServiceImplStartupTest, StartCrosNoCredentials) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // On ChromeOS, the user is always immediately signed in, but a refresh token
  // isn't necessarily available yet.
  SimulateRefreshTokensNotLoadedYet();
  SimulateTestUserSigninWithoutRefreshToken();

  CreateSyncService(SyncServiceImpl::AUTO_START);

  // Calling Initialize should cause the service to immediately create and
  // initialize the engine, and configure the DataTypeManager.
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());

  // Sync should be considered active, even though there is no refresh token.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Since we're in AUTO_START mode, FirstSetupComplete gets set automatically.
  EXPECT_TRUE(sync_service()->GetUserSettings()->IsFirstSetupComplete());
}

TEST_F(SyncServiceImplStartupTest, StartCrosFirstTime) {
  // We've never completed Sync startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // There is already a signed-in user.
  SimulateTestUserSignin();

  // Sync should become active, even though IsFirstSetupComplete wasn't set yet,
  // due to AUTO_START.
  CreateSyncService(SyncServiceImpl::AUTO_START);
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

TEST_F(SyncServiceImplStartupTest, StartNormal) {
  // We have previously completed the initial Sync setup, and the user is
  // already signed in.
  sync_prefs()->SetFirstSetupComplete();
  SimulateTestUserSignin();

  CreateSyncService(SyncServiceImpl::MANUAL_START);

  // Since all conditions for starting Sync are already fulfilled, calling
  // Initialize should immediately create and initialize the engine and
  // configure the DataTypeManager. In this test, all of these operations are
  // synchronous.
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(nullptr, data_type_manager());
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

TEST_F(SyncServiceImplStartupTest, StopSync) {
  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(SyncServiceImpl::MANUAL_START);
  SimulateTestUserSignin();

  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  // On SetSyncRequested(false), the sync service will immediately start up
  // again in transport mode.
  sync_service()->GetUserSettings()->SetSyncRequested(false);
  base::RunLoop().RunUntilIdle();

  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

TEST_F(SyncServiceImplStartupTest, DisableSync) {
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();
  SimulateTestUserSignin();
  CreateSyncService(SyncServiceImpl::MANUAL_START);

  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(sync_service()->IsSyncFeatureActive());
  ASSERT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  // On StopAndClear(), the sync service will immediately start up again in
  // transport mode.
  sync_service()->StopAndClear();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());

  // Call StopAndClear() again while the sync service is already in transport
  // mode. It should immediately start up again in transport mode.
  sync_service()->StopAndClear();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DataTypeManager::CONFIGURED, data_type_manager()->state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(SyncServiceImplStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  pref_service()->ClearPref(prefs::kSyncKeepEverythingSynced);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    pref_service()->ClearPref(SyncPrefs::GetPrefNameForType(type));
  }

  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(SyncServiceImpl::MANUAL_START);
  SimulateTestUserSignin();

  sync_service()->Initialize();

  EXPECT_TRUE(sync_prefs()->HasKeepEverythingSynced());
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
TEST_F(SyncServiceImplStartupTest, StartDontRecoverDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  sync_prefs()->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*choosable_types=*/UserSelectableTypeSet::All(),
      /*chosen_types=*/{UserSelectableType::kBookmarks});

  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(SyncServiceImpl::MANUAL_START);
  SimulateTestUserSignin();

  sync_service()->Initialize();

  EXPECT_FALSE(sync_prefs()->HasKeepEverythingSynced());
}

TEST_F(SyncServiceImplStartupTest, ManagedStartup) {
  // Sync was previously enabled, but a policy was set while Chrome wasn't
  // running.
  pref_service()->SetBoolean(prefs::kSyncManaged, true);
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();

  SimulateTestUserSignin();
  CreateSyncService(SyncServiceImpl::MANUAL_START);

  sync_service()->Initialize();
  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            sync_service()->GetDisableReasons());
  // Service should not be started by Initialize() since it's managed.
  EXPECT_EQ(nullptr, data_type_manager());
  EXPECT_FALSE(engine());
}

TEST_F(SyncServiceImplStartupTest, SwitchManaged) {
  // Sync starts out fully set up and enabled.
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();
  SimulateTestUserSignin();
  CreateSyncService(SyncServiceImpl::MANUAL_START);

  // Initialize() should be enough to kick off Sync startup (which is instant in
  // this test).
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::DisableReasonSet(),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

  // The service should stop when switching to managed mode.
  pref_service()->SetBoolean(prefs::kSyncManaged, true);
  // Give re-startup a chance to happen (it shouldn't!).
  base::RunLoop().RunUntilIdle();
  // Sync was disabled due to the policy, setting SyncRequested to false and
  // causing DISABLE_REASON_USER_CHOICE.
  ASSERT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
                SyncService::DISABLE_REASON_USER_CHOICE),
            sync_service()->GetDisableReasons());
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
  EXPECT_EQ(1, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

  // When switching back to unmanaged, Sync-the-transport should start up
  // automatically, which causes (re)creation of SyncEngine and
  // DataTypeManager.
  pref_service()->SetBoolean(prefs::kSyncManaged, false);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      sync_service()->GetDisableReasons());

  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still considered off because disabling Sync through
  // policy also reset the sync-requested and first-setup-complete flags.
  EXPECT_FALSE(sync_service()->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(SyncServiceImplStartupTest, StartDownloadFailed) {
  sync_prefs()->SetSyncRequested(true);
  CreateSyncService(SyncServiceImpl::MANUAL_START);
  SimulateTestUserSignin();
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // Prevent automatic (and successful) completion of engine initialization.
  component_factory()->AllowFakeEngineInitCompletion(false);
  sync_service()->Initialize();
  base::RunLoop().RunUntilIdle();

  // Simulate a failure while downloading control types.
  engine()->TriggerInitializationCompletion(/*success=*/false);

  std::unique_ptr<SyncSetupInProgressHandle> sync_blocker =
      sync_service()->GetSetupInProgressHandle();
  sync_blocker.reset();
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
}

// ChromeOS does not support sign-in after startup
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplStartupTest, FullStartupSequenceFirstTime) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // Note: Deferred startup is only enabled if SESSIONS is among the preferred
  // data types.
  CreateSyncService(SyncServiceImpl::MANUAL_START,
                    ModelTypeSet(SESSIONS, TYPED_URLS));
  sync_service()->Initialize();
  ASSERT_FALSE(sync_service()->CanSyncFeatureStart());

  // There is no signed-in user, so also nobody has decided that Sync should be
  // started.
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Sign in. Now Sync-the-transport can start. Since this was triggered by an
  // explicit user event, deferred startup is bypassed.
  // Sync-the-feature still doesn't start until the user says they want it.
  component_factory()->AllowFakeEngineInitCompletion(false);
  SimulateTestUserSignin();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  ASSERT_TRUE(engine());

  // Initiate Sync (the feature) setup before the engine initializes itself in
  // transport mode.
  sync_service()->GetUserSettings()->SetSyncRequested(true);
  std::unique_ptr<SyncSetupInProgressHandle> setup_in_progress_handle =
      sync_service()->GetSetupInProgressHandle();

  // Once the engine calls back and says it's initialized, we're just waiting
  // for the user to finish the initial configuration (choosing data types etc.)
  // before actually syncing data.
  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Once the user finishes the initial setup, the service can actually start
  // configuring the data types. Just marking the initial setup as complete
  // isn't enough though, because setup is still considered in progress (we
  // haven't released the setup-in-progress handle).
  sync_service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  EXPECT_EQ(SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());

  // Prevent immediate configuration of one datatype, to verify the state
  // during CONFIGURING.
  ASSERT_EQ(DataTypeController::NOT_RUNNING, get_controller(SESSIONS)->state());
  get_controller(SESSIONS)->model()->EnableManualModelStart();

  // Releasing the setup in progress handle lets the service actually configure
  // the DataTypeManager.
  setup_in_progress_handle.reset();

  // While DataTypeManager configuration is ongoing, the overall state is still
  // CONFIGURING.
  EXPECT_EQ(SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  EXPECT_NE(nullptr, data_type_manager());
  EXPECT_TRUE(engine());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  get_controller(SESSIONS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplStartupTest, FullStartupSequenceNthTime) {
  // The user is already signed in and has completed Sync setup before.
  SimulateTestUserSignin();
  sync_prefs()->SetFirstSetupComplete();
  sync_prefs()->SetSyncRequested(true);

  // Note: Deferred startup is only enabled if SESSIONS is among the preferred
  // data types.
  CreateSyncService(SyncServiceImpl::MANUAL_START,
                    ModelTypeSet(SESSIONS, TYPED_URLS));
  sync_service()->Initialize();
  ASSERT_TRUE(sync_service()->CanSyncFeatureStart());

  // Nothing is preventing Sync from starting, but it should be deferred so as
  // to now slow down browser startup.
  EXPECT_EQ(SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());
  EXPECT_EQ(nullptr, data_type_manager());
  EXPECT_FALSE(engine());

  // Wait for the deferred startup timer to expire. The Sync service will start
  // and initialize the engine.
  component_factory()->AllowFakeEngineInitCompletion(false);
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());
  EXPECT_EQ(nullptr, data_type_manager());
  EXPECT_TRUE(engine());

  // Prevent immediate configuration of one datatype, to verify the state
  // during CONFIGURING.
  ASSERT_EQ(DataTypeController::NOT_RUNNING, get_controller(SESSIONS)->state());
  get_controller(SESSIONS)->model()->EnableManualModelStart();

  // Once the engine calls back and says it's initialized, the DataTypeManager
  // will start configuring, since initial setup is already done.
  engine()->TriggerInitializationCompletion(/*success=*/true);

  ASSERT_EQ(DataTypeController::MODEL_STARTING,
            get_controller(SESSIONS)->state());
  EXPECT_NE(nullptr, data_type_manager());
  EXPECT_TRUE(engine());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  get_controller(SESSIONS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

}  // namespace syncer
