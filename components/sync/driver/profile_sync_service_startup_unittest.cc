// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/profile_sync_service.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/data_type_manager_mock.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/profile_sync_service_bundle.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/fake_sync_engine.h"
#include "components/sync/engine/mock_sync_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace syncer {

namespace {

const char kEmail[] = "test_user@gmail.com";

void SetError(DataTypeManager::ConfigureResult* result) {
  DataTypeStatusTable::TypeErrorMap errors;
  errors[BOOKMARKS] =
      SyncError(FROM_HERE, SyncError::UNRECOVERABLE_ERROR, "Error", BOOKMARKS);
  result->data_type_status_table.UpdateFailedDataTypes(errors);
}

}  // namespace

ACTION_P(InvokeOnConfigureStart, sync_service) {
  sync_service->OnConfigureStart();
}

ACTION_P3(InvokeOnConfigureDone, sync_service, error_callback, result) {
  DataTypeManager::ConfigureResult configure_result =
      static_cast<DataTypeManager::ConfigureResult>(result);
  if (result.status == DataTypeManager::ABORTED)
    error_callback.Run(&configure_result);
  sync_service->OnConfigureDone(configure_result);
}

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        sync_prefs_(profile_sync_service_bundle_.pref_service()) {
    profile_sync_service_bundle_.identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  ~ProfileSyncServiceStartupTest() override { sync_service_->Shutdown(); }

  void CreateSyncService(
      ProfileSyncService::StartBehavior start_behavior,
      ModelTypeSet registered_types = ModelTypeSet(BOOKMARKS)) {
    DataTypeController::TypeVector controllers;
    for (ModelType type : registered_types) {
      controllers.push_back(std::make_unique<FakeDataTypeController>(type));
    }

    std::unique_ptr<SyncClientMock> sync_client =
        profile_sync_service_bundle_.CreateSyncClientMock();
    ON_CALL(*sync_client, CreateDataTypeControllers(_))
        .WillByDefault(Return(ByMove(std::move(controllers))));

    sync_service_ = std::make_unique<ProfileSyncService>(
        profile_sync_service_bundle_.CreateBasicInitParams(
            start_behavior, std::move(sync_client)));
  }

  void SimulateTestUserSignin() {
    profile_sync_service_bundle_.identity_test_env()
        ->MakePrimaryAccountAvailable(kEmail);
  }

  void SimulateTestUserSigninWithoutRefreshToken() {
    // Set the primary account *without* providing an OAuth token.
    profile_sync_service_bundle_.identity_test_env()->SetPrimaryAccount(kEmail);
  }

  void UpdateCredentials() {
    profile_sync_service_bundle_.identity_test_env()
        ->SetRefreshTokenForPrimaryAccount();
  }

  DataTypeManagerMock* SetUpDataTypeManagerMock() {
    auto data_type_manager = std::make_unique<NiceMock<DataTypeManagerMock>>();
    DataTypeManagerMock* data_type_manager_raw = data_type_manager.get();
    ON_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
        .WillByDefault(Return(ByMove(std::move(data_type_manager))));
    return data_type_manager_raw;
  }

  FakeSyncEngine* SetUpFakeSyncEngine() {
    auto sync_engine = std::make_unique<FakeSyncEngine>();
    FakeSyncEngine* sync_engine_raw = sync_engine.get();
    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
        .WillByDefault(Return(ByMove(std::move(sync_engine))));
    return sync_engine_raw;
  }

  MockSyncEngine* SetUpMockSyncEngine() {
    auto sync_engine = std::make_unique<NiceMock<MockSyncEngine>>();
    MockSyncEngine* sync_engine_raw = sync_engine.get();
    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
        .WillByDefault(Return(ByMove(std::move(sync_engine))));
    return sync_engine_raw;
  }

  SyncPrefs* sync_prefs() { return &sync_prefs_; }

  ProfileSyncService* sync_service() { return sync_service_.get(); }

  PrefService* pref_service() {
    return profile_sync_service_bundle_.pref_service();
  }

  SyncApiComponentFactoryMock* component_factory() {
    return profile_sync_service_bundle_.component_factory();
  }

  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ProfileSyncServiceBundle profile_sync_service_bundle_;
  SyncPrefs sync_prefs_;
  std::unique_ptr<ProfileSyncService> sync_service_;
};

// ChromeOS does not support sign-in after startup (in particular,
// IdentityManager::Observer::OnPrimaryAccountSet never gets called).
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  CreateSyncService(ProfileSyncService::MANUAL_START);
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::STOPPED));

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  sync_service()->Initialize();
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
                                    SyncService::DISABLE_REASON_USER_CHOICE),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Preferences should be back to defaults.
  EXPECT_EQ(base::Time(), sync_prefs()->GetLastSyncedTime());
  EXPECT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // This tells the ProfileSyncService that setup is now in progress, which
  // causes it to try starting up the engine. We're not signed in yet though, so
  // that won't work.
  sync_service()->GetUserSettings()->SetSyncRequested(true);
  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_NOT_SIGNED_IN),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  SimulateTestUserSignin();

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
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_blocker.reset();
  ASSERT_FALSE(sync_service()->IsSetupInProgress());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still not active, but rather pending confirmation.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());

  // Marking first setup complete will let ProfileSyncService reconfigure the
  // DataTypeManager in full Sync-the-feature mode.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  // This should have fully enabled sync.
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  EXPECT_CALL(*data_type_manager, Stop(BROWSER_SHUTDOWN));
}
#endif  // OS_CHROMEOS

TEST_F(ProfileSyncServiceStartupTest, StartNoCredentials) {
  // We're already signed in, but don't have a refresh token.
  SimulateTestUserSigninWithoutRefreshToken();
  sync_prefs()->SetFirstSetupComplete();

  CreateSyncService(ProfileSyncService::MANUAL_START);

  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  sync_service()->Initialize();

  // ProfileSyncService should now be active, but of course not have an access
  // token.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->GetAccessTokenForTest().empty());
  // Note that ProfileSyncService is not in an auth error state - no auth was
  // attempted, so no error.
}

TEST_F(ProfileSyncServiceStartupTest, StartInvalidCredentials) {
  SimulateTestUserSignin();
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();

  CreateSyncService(ProfileSyncService::MANUAL_START);

  // Tell the engine to stall while downloading control types (simulating an
  // auth error).
  FakeSyncEngine* fake_engine = SetUpFakeSyncEngine();
  fake_engine->set_fail_initial_download(true);
  // Note: Since engine initialization will fail, the DataTypeManager should not
  // get created at all here.

  sync_service()->Initialize();

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

TEST_F(ProfileSyncServiceStartupTest, StartCrosNoCredentials) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // On ChromeOS, the user is always immediately signed in, but a refresh token
  // isn't necessarily available yet.
  SimulateTestUserSigninWithoutRefreshToken();

  CreateSyncService(ProfileSyncService::AUTO_START);

  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();

  // Calling Initialize should cause the service to immediately create and
  // initialize the engine, and configure the DataTypeManager.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->Initialize();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  // Sync should be considered active, even though there is no refresh token.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Since we're in AUTO_START mode, FirstSetupComplete gets set automatically.
  EXPECT_TRUE(sync_service()->GetUserSettings()->IsFirstSetupComplete());
}

TEST_F(ProfileSyncServiceStartupTest, StartCrosFirstTime) {
  // On ChromeOS, the user is always immediately signed in, but a refresh token
  // isn't necessarily available yet.
  SimulateTestUserSigninWithoutRefreshToken();

  CreateSyncService(ProfileSyncService::AUTO_START);

  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  // The primary account is already populated, all that's left to do is provide
  // a refresh token.
  UpdateCredentials();
  sync_service()->Initialize();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_CALL(*data_type_manager, Stop(BROWSER_SHUTDOWN));
}

TEST_F(ProfileSyncServiceStartupTest, StartNormal) {
  // We have previously completed the initial Sync setup, and the user is
  // already signed in.
  sync_prefs()->SetFirstSetupComplete();
  SimulateTestUserSignin();

  CreateSyncService(ProfileSyncService::MANUAL_START);
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  // Since all conditions for starting Sync are already fulfilled, calling
  // Initialize should immediately create and initialize the engine and
  // configure the DataTypeManager. In this test, all of these operations are
  // synchronous.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->Initialize();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  EXPECT_CALL(*data_type_manager, Stop(BROWSER_SHUTDOWN));
}

TEST_F(ProfileSyncServiceStartupTest, StopSync) {
  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_CALL(*data_type_manager, Stop(STOP_SYNC));
  // On SetSyncRequested(false), the sync service will immediately start up
  // again in transport mode.
  SetUpFakeSyncEngine();
  data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->GetUserSettings()->SetSyncRequested(false);

  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceStartupTest, DisableSync) {
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();
  SimulateTestUserSignin();
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();
  ASSERT_TRUE(sync_service()->IsSyncFeatureActive());

  // On StopAndClear(), the sync service will immediately start up again in
  // transport mode.
  SetUpFakeSyncEngine();
  data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->StopAndClear();

  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());

  // Call StopAndClear() again while the sync service is already in transport
  // mode. It should immediately start up again in transport mode.
  SetUpFakeSyncEngine();
  data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->StopAndClear();
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  pref_service()->ClearPref(prefs::kSyncKeepEverythingSynced);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    pref_service()->ClearPref(SyncPrefs::GetPrefNameForType(type));
  }

  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_TRUE(sync_prefs()->HasKeepEverythingSynced());
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
TEST_F(ProfileSyncServiceStartupTest, StartDontRecoverDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  sync_prefs()->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*choosable_types=*/UserSelectableTypeSet::All(),
      /*chosen_types=*/{UserSelectableType::kBookmarks});

  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_FALSE(sync_prefs()->HasKeepEverythingSynced());
}

TEST_F(ProfileSyncServiceStartupTest, ManagedStartup) {
  // Sync is enabled by the user, but disabled by policy.
  sync_prefs()->SetManagedForTest(true);
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();

  SimulateTestUserSignin();
  CreateSyncService(ProfileSyncService::MANUAL_START);

  // Service should not be started by Initialize() since it's managed.
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _)).Times(0);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .Times(0);
  sync_service()->Initialize();
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY),
            sync_service()->GetDisableReasons());
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  // Sync starts out fully set up and enabled.
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();
  SimulateTestUserSignin();
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  // Initialize() should be enough to kick off Sync startup (which is instant in
  // this test).
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::DisableReasonSet(),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop(DISABLE_SYNC));

  sync_prefs()->SetManagedForTest(true);
  ASSERT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY),
            sync_service()->GetDisableReasons());
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
  // Note that PSS no longer references |data_type_manager| after stopping.

  // When switching back to unmanaged, Sync-the-transport should start up
  // automatically, which causes (re)creation of SyncEngine and
  // DataTypeManager.
  SetUpFakeSyncEngine();
  Mock::VerifyAndClearExpectations(data_type_manager);
  data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  sync_prefs()->SetManagedForTest(false);

  ASSERT_EQ(SyncService::DisableReasonSet(),
            sync_service()->GetDisableReasons());

  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still considered off because disabling Sync through
  // policy also reset the first-setup-complete flag.
  EXPECT_FALSE(sync_service()->GetUserSettings()->IsFirstSetupComplete());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  sync_prefs()->SetSyncRequested(true);
  sync_prefs()->SetFirstSetupComplete();
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  DataTypeManager::ConfigureResult result(status, ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_, _))
      .WillRepeatedly(
          DoAll(InvokeOnConfigureStart(sync_service()),
                InvokeOnConfigureDone(sync_service(),
                                      base::BindRepeating(&SetError), result)));
  EXPECT_CALL(*data_type_manager, state())
      .WillOnce(Return(DataTypeManager::STOPPED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->HasUnrecoverableError());
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR),
            sync_service()->GetDisableReasons());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  sync_prefs()->SetSyncRequested(true);
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  FakeSyncEngine* fake_engine = SetUpFakeSyncEngine();
  fake_engine->set_fail_initial_download(true);

  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  sync_service()->Initialize();

  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  sync_blocker.reset();
  EXPECT_EQ(SyncService::DisableReasonSet(
                SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
}

// ChromeOS does not support sign-in after startup (in particular,
// IdentityManager::Observer::OnPrimaryAccountSet never gets called).
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceStartupTest, FullStartupSequenceFirstTime) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  MockSyncEngine* sync_engine = SetUpMockSyncEngine();
  EXPECT_CALL(*sync_engine, Initialize(_)).Times(0);

  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::STOPPED));

  // Note: Deferred startup is only enabled if SESSIONS is among the preferred
  // data types.
  CreateSyncService(ProfileSyncService::MANUAL_START,
                    ModelTypeSet(SESSIONS, TYPED_URLS));
  sync_service()->Initialize();

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
  EXPECT_CALL(*sync_engine, Initialize(_));
  SimulateTestUserSignin();
  EXPECT_EQ(
      SyncService::DisableReasonSet(SyncService::DISABLE_REASON_USER_CHOICE),
      sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Initiate Sync (the feature) setup before the engine initializes itself in
  // transport mode.
  sync_service()->GetUserSettings()->SetSyncRequested(true);
  auto setup_in_progress_handle = sync_service()->GetSetupInProgressHandle();

  // Once the engine calls back and says it's initialized, we're just waiting
  // for the user to finish the initial configuration (choosing data types etc.)
  // before actually syncing data.
  ON_CALL(*sync_engine, IsInitialized()).WillByDefault(Return(true));
  sync_service()->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                                      WeakHandle<DataTypeDebugInfoListener>(),
                                      "test-birthday", "test-bag-of-chips",
                                      /*success=*/true);
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

  // Releasing the setup in progress handle lets the service actually configure
  // the DataTypeManager.
  EXPECT_CALL(*data_type_manager, Configure(_, _))
      .WillOnce(InvokeWithoutArgs(sync_service(),
                                  &ProfileSyncService::OnConfigureStart));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURING));
  setup_in_progress_handle.reset();
  // While DataTypeManager configuration is ongoing, the overall state is still
  // CONFIGURING.
  EXPECT_EQ(SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  DataTypeManager::ConfigureResult configure_result(DataTypeManager::OK,
                                                    ModelTypeSet(SESSIONS));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->OnConfigureDone(configure_result);
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
}
#endif  // OS_CHROMEOS

TEST_F(ProfileSyncServiceStartupTest, FullStartupSequenceNthTime) {
  // The user is already signed in and has completed Sync setup before.
  SimulateTestUserSignin();
  sync_prefs()->SetFirstSetupComplete();

  MockSyncEngine* sync_engine = SetUpMockSyncEngine();
  EXPECT_CALL(*sync_engine, Initialize(_)).Times(0);

  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::STOPPED));

  // Note: Deferred startup is only enabled if SESSIONS is among the preferred
  // data types.
  CreateSyncService(ProfileSyncService::MANUAL_START,
                    ModelTypeSet(SESSIONS, TYPED_URLS));
  sync_service()->Initialize();

  // Nothing is preventing Sync from starting, but it should be deferred so as
  // to now slow down browser startup.
  EXPECT_EQ(SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());

  // Wait for the deferred startup timer to expire. The Sync service will start
  // and initialize the engine.
  EXPECT_CALL(*sync_engine, Initialize(_));
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());

  // Once the engine calls back and says it's initialized, the DataTypeManager
  // will get configured, since initial setup is already done.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*sync_engine, IsInitialized()).WillByDefault(Return(true));
  sync_service()->OnEngineInitialized(ModelTypeSet(), WeakHandle<JsBackend>(),
                                      WeakHandle<DataTypeDebugInfoListener>(),
                                      "test-birthday", "test-bag-of-chips",
                                      /*success=*/true);
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURING));
  EXPECT_EQ(SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  DataTypeManager::ConfigureResult configure_result(DataTypeManager::OK,
                                                    ModelTypeSet(SESSIONS));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->OnConfigureDone(configure_result);
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

}  // namespace syncer
