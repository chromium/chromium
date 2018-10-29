// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/data_type_manager_mock.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/fake_sync_engine.h"
#include "components/sync/engine/mock_sync_engine.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DataTypeManager;
using syncer::DataTypeManagerMock;
using syncer::FakeSyncEngine;
using syncer::MockSyncEngine;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace browser_sync {

namespace {

const char kEmail[] = "test_user@gmail.com";

void SetError(DataTypeManager::ConfigureResult* result) {
  syncer::DataTypeStatusTable::TypeErrorMap errors;
  errors[syncer::BOOKMARKS] =
      syncer::SyncError(FROM_HERE, syncer::SyncError::UNRECOVERABLE_ERROR,
                        "Error", syncer::BOOKMARKS);
  result->data_type_status_table.UpdateFailedDataTypes(errors);
}

}  // namespace

ACTION_P(InvokeOnConfigureStart, sync_service) {
  sync_service->OnConfigureStart();
}

ACTION_P3(InvokeOnConfigureDone, sync_service, error_callback, result) {
  DataTypeManager::ConfigureResult configure_result =
      static_cast<DataTypeManager::ConfigureResult>(result);
  if (result.status == syncer::DataTypeManager::ABORTED)
    error_callback.Run(&configure_result);
  sync_service->OnConfigureDone(configure_result);
}

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        sync_prefs_(profile_sync_service_bundle_.pref_service()) {
    profile_sync_service_bundle_.auth_service()
        ->set_auto_post_fetch_response_on_message_loop(true);
  }

  ~ProfileSyncServiceStartupTest() override {
    sync_service_->Shutdown();
  }

  void CreateSyncService(ProfileSyncService::StartBehavior start_behavior,
                         syncer::ModelTypeSet registered_types =
                             syncer::ModelTypeSet(syncer::BOOKMARKS)) {
    ProfileSyncServiceBundle::SyncClientBuilder builder(
        &profile_sync_service_bundle_);
    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(start_behavior,
                                                           builder.Build());

    ON_CALL(*component_factory(), CreateCommonDataTypeControllers(_, _))
        .WillByDefault(InvokeWithoutArgs([=]() {
          syncer::DataTypeController::TypeVector controllers;
          for (syncer::ModelType type : registered_types) {
            controllers.push_back(
                std::make_unique<syncer::FakeDataTypeController>(type));
          }
          return controllers;
        }));

    sync_service_ =
        std::make_unique<ProfileSyncService>(std::move(init_params));
  }

  void SimulateTestUserSignin() {
    identity::MakePrimaryAccountAvailable(
        profile_sync_service_bundle_.signin_manager(),
        profile_sync_service_bundle_.auth_service(),
        profile_sync_service_bundle_.identity_manager(), kEmail);
  }

  void SimulateTestUserSigninWithoutRefreshToken() {
    // Set the primary account *without* providing an OAuth token.
    identity::SetPrimaryAccount(profile_sync_service_bundle_.signin_manager(),
                                profile_sync_service_bundle_.identity_manager(),
                                kEmail);
  }

  void UpdateCredentials() {
    identity::SetRefreshTokenForPrimaryAccount(
        profile_sync_service_bundle_.auth_service(),
        profile_sync_service_bundle_.identity_manager());
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

  syncer::SyncPrefs* sync_prefs() { return &sync_prefs_; }

  ProfileSyncService* sync_service() { return sync_service_.get(); }

  PrefService* pref_service() {
    return profile_sync_service_bundle_.pref_service();
  }

  syncer::SyncApiComponentFactoryMock* component_factory() {
    return profile_sync_service_bundle_.component_factory();
  }

  void FastForwardUntilNoTasksRemain() {
    scoped_task_environment_.FastForwardUntilNoTasksRemain();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ProfileSyncServiceBundle profile_sync_service_bundle_;
  syncer::SyncPrefs sync_prefs_;
  std::unique_ptr<ProfileSyncService> sync_service_;
};

class ProfileSyncServiceWithStandaloneTransportStartupTest
    : public ProfileSyncServiceStartupTest {
 protected:
  ProfileSyncServiceWithStandaloneTransportStartupTest() {
    feature_list_.InitAndEnableFeature(switches::kSyncStandaloneTransport);
  }

  ~ProfileSyncServiceWithStandaloneTransportStartupTest() override {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ProfileSyncServiceWithoutStandaloneTransportStartupTest
    : public ProfileSyncServiceStartupTest {
 protected:
  ProfileSyncServiceWithoutStandaloneTransportStartupTest() {
    feature_list_.InitAndDisableFeature(switches::kSyncStandaloneTransport);
  }

  ~ProfileSyncServiceWithoutStandaloneTransportStartupTest() override {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

// ChromeOS does not support sign-in after startup (in particular,
// IdentityManager::Observer::OnPrimaryAccountSet never gets called).
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceWithoutStandaloneTransportStartupTest,
       StartFirstTime) {
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
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Preferences should be back to defaults.
  EXPECT_EQ(base::Time(), sync_prefs()->GetLastSyncedTime());
  EXPECT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // Confirmation isn't needed before sign in occurs.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  // This tells the ProfileSyncService that setup is now in progress, which
  // causes it to try starting up the engine. We're not signed in yet though, so
  // that won't work.
  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Confirmation isn't needed before sign in occurs, or when setup is already
  // in progress.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  // Simulate successful signin. This will cause ProfileSyncService to start,
  // since all conditions are now fulfilled.
  SimulateTestUserSignin();

  // Now we're signed in, so the engine can start. There's already a setup in
  // progress, so we don't go into the WAITING_FOR_START_REQUEST state. Engine
  // initialization is immediate in this test, so we also bypass the
  // INITIALIZING state.
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NONE,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());

  // Setup is already in progress, so confirmation still isn't needed.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  // Simulate the UI telling sync it has finished setting up. Note that this is
  // a two-step process: Releasing the SetupInProgressHandle, and marking first
  // setup complete.
  sync_blocker.reset();
  // Now setup isn't in progress anymore, but Sync is still waiting to be told
  // that the initial setup was completed.
  ASSERT_FALSE(sync_service()->IsSetupInProgress());
  EXPECT_TRUE(sync_service()->IsSyncConfirmationNeeded());
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());

  // Marking first setup complete will let ProfileSyncService configure the
  // DataTypeManager.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->SetFirstSetupComplete();

  // This should have fully enabled sync.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
}

TEST_F(ProfileSyncServiceWithStandaloneTransportStartupTest, StartFirstTime) {
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
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Preferences should be back to defaults.
  EXPECT_EQ(base::Time(), sync_prefs()->GetLastSyncedTime());
  EXPECT_FALSE(sync_prefs()->IsFirstSetupComplete());

  // Confirmation isn't needed before sign in occurs.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  // This tells the ProfileSyncService that setup is now in progress, which
  // causes it to try starting up the engine. We're not signed in yet though, so
  // that won't work.
  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Confirmation isn't needed before sign in occurs, or when setup is already
  // in progress.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  SimulateTestUserSignin();

  // Now we're signed in, so the engine can start. There's already a setup in
  // progress, so we don't go into the WAITING_FOR_START_REQUEST state. Engine
  // initialization is immediate in this test, so we also bypass the
  // INITIALIZING state.
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NONE,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());

  // Setup is already in progress, so confirmation still isn't needed.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

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
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still not active, but rather pending confirmation.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
  EXPECT_TRUE(sync_service()->IsSyncConfirmationNeeded());

  // Marking first setup complete will let ProfileSyncService reconfigure the
  // DataTypeManager in full Sync-the-feature mode.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->SetFirstSetupComplete();

  // This should have fully enabled sync.
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
}
#endif  // OS_CHROMEOS

TEST_F(ProfileSyncServiceStartupTest, StartNoCredentials) {
  // We're already signed in, but don't have a refresh token.
  SimulateTestUserSigninWithoutRefreshToken();

  CreateSyncService(ProfileSyncService::MANUAL_START);

  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  sync_service()->Initialize();

  // ProfileSyncService should now be active, but of course not have an access
  // token.
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->GetAccessTokenForTest().empty());
  // Note that ProfileSyncService is not in an auth error state - no auth was
  // attempted, so no error.
}

TEST_F(ProfileSyncServiceStartupTest, StartInvalidCredentials) {
  SimulateTestUserSignin();

  CreateSyncService(ProfileSyncService::MANUAL_START);

  sync_service()->SetFirstSetupComplete();

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
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
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
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Since we're in AUTO_START mode, FirstSetupComplete gets set automatically.
  EXPECT_TRUE(sync_service()->IsFirstSetupComplete());
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
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
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

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
}

TEST_F(ProfileSyncServiceWithoutStandaloneTransportStartupTest, StopSync) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_CALL(*data_type_manager, Stop(syncer::STOP_SYNC));
  sync_service()->RequestStop(syncer::SyncService::KEEP_DATA);

  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceWithStandaloneTransportStartupTest, StopSync) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_CALL(*data_type_manager, Stop(syncer::STOP_SYNC));
  // On RequestStop(), the sync service will immediately start up again in
  // transport mode.
  SetUpFakeSyncEngine();
  data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->RequestStop(syncer::SyncService::KEEP_DATA);

  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceWithoutStandaloneTransportStartupTest, DisableSync) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_CALL(*data_type_manager, Stop(syncer::DISABLE_SYNC));
  sync_service()->RequestStop(syncer::SyncService::CLEAR_DATA);

  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceWithStandaloneTransportStartupTest, DisableSync) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  // On RequestStop(), the sync service will immediately start up again in
  // transport mode.
  SetUpFakeSyncEngine();
  data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->RequestStop(syncer::SyncService::CLEAR_DATA);

  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  pref_service()->ClearPref(syncer::prefs::kSyncKeepEverythingSynced);
  syncer::ModelTypeSet user_types = syncer::UserTypes();
  for (syncer::ModelType type : user_types) {
    pref_service()->ClearPref(syncer::SyncPrefs::GetPrefNameForDataType(type));
  }

  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
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
  sync_prefs()->SetKeepEverythingSynced(false);

  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
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
  // Service should not be started by Initialize() since it's managed.
  SimulateTestUserSignin();
  CreateSyncService(ProfileSyncService::MANUAL_START);

  // Disable sync through policy.
  sync_prefs()->SetManagedForTest(true);

  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _)).Times(0);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .Times(0);
  sync_service()->Initialize();
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
            sync_service()->GetDisableReasons());
}

TEST_F(ProfileSyncServiceWithoutStandaloneTransportStartupTest, SwitchManaged) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NONE,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop(syncer::DISABLE_SYNC));
  sync_prefs()->SetManagedForTest(true);
  ASSERT_EQ(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
            sync_service()->GetDisableReasons());
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
  // Note that PSS no longer references |data_type_manager| after stopping.

  // When switching back to unmanaged, the state should change but sync should
  // not start automatically because IsFirstSetupComplete() will be false and
  // no setup is in progress.
  // A new DataTypeManager should not be created.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .Times(0);
  sync_prefs()->SetManagedForTest(false);
  ASSERT_EQ(syncer::SyncService::DISABLE_REASON_NONE,
            sync_service()->GetDisableReasons());
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::TransportState::WAITING_FOR_START_REQUEST,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceWithStandaloneTransportStartupTest, SwitchManaged) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NONE,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop(syncer::DISABLE_SYNC));

  sync_prefs()->SetManagedForTest(true);
  ASSERT_EQ(syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
            sync_service()->GetDisableReasons());
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
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

  ASSERT_EQ(syncer::SyncService::DISABLE_REASON_NONE,
            sync_service()->GetDisableReasons());

  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still considered off.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  DataTypeManager::ConfigureResult result(status, syncer::ModelTypeSet());
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
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR,
            sync_service()->GetDisableReasons());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  FakeSyncEngine* fake_engine = SetUpFakeSyncEngine();
  fake_engine->set_fail_initial_download(true);

  ASSERT_FALSE(sync_prefs()->IsFirstSetupComplete());

  sync_service()->Initialize();

  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  sync_blocker.reset();
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
}

// ChromeOS does not support sign-in after startup (in particular,
// IdentityManager::Observer::OnPrimaryAccountSet never gets called).
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceWithoutStandaloneTransportStartupTest,
       FullStartupSequenceFirstTime) {
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
                    syncer::ModelTypeSet(syncer::SESSIONS));
  sync_service()->Initialize();

  // There is no signed-in user, but nothing else prevents Sync from starting.
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Sign in. Now Sync is ready to start, just waiting for a prod.
  SimulateTestUserSignin();
  EXPECT_EQ(syncer::SyncService::TransportState::WAITING_FOR_START_REQUEST,
            sync_service()->GetTransportState());

  // Once we give the service a prod by initiating Sync setup, it'll start and
  // initialize the engine. Since this is the initial Sync start, this will not
  // be deferred.
  EXPECT_CALL(*sync_engine, Initialize(_));
  auto setup_in_progress_handle = sync_service()->GetSetupInProgressHandle();
  EXPECT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());

  // Once the engine calls back and says it's initialized, we're just waiting
  // for the user to finish the initial configuration (choosing data types etc.)
  // before actually syncing data.
  sync_service()->OnEngineInitialized(
      syncer::ModelTypeSet(), syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(), "test-guid",
      "test-session-name", /*success=*/true);
  ASSERT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Once the user finishes the initial setup, the service can actually start
  // configuring the data types. Just marking the initial setup as complete
  // isn't enough though, because setup is still considered in progress (we
  // haven't released the setup-in-progress handle).
  sync_service()->SetFirstSetupComplete();
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
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
  EXPECT_EQ(syncer::SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  DataTypeManager::ConfigureResult configure_result(
      DataTypeManager::OK, syncer::ModelTypeSet(syncer::SESSIONS));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->OnConfigureDone(configure_result);
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
}

TEST_F(ProfileSyncServiceWithStandaloneTransportStartupTest,
       FullStartupSequenceFirstTime) {
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
                    syncer::ModelTypeSet(syncer::SESSIONS));
  sync_service()->Initialize();

  // There is no signed-in user, but nothing else prevents Sync from starting.
  EXPECT_EQ(syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            sync_service()->GetDisableReasons());
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Sign in. Now Sync is ready to start, just waiting for a prod.
  SimulateTestUserSignin();
  EXPECT_EQ(syncer::SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());

  // Once we give the service a prod by initiating Sync setup, it'll start and
  // initialize the engine. Since this is the initial Sync start, this will not
  // be deferred.
  EXPECT_CALL(*sync_engine, Initialize(_));
  auto setup_in_progress_handle = sync_service()->GetSetupInProgressHandle();
  EXPECT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());

  // Once the engine calls back and says it's initialized, we're just waiting
  // for the user to finish the initial configuration (choosing data types etc.)
  // before actually syncing data.
  sync_service()->OnEngineInitialized(
      syncer::ModelTypeSet(), syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(), "test-guid",
      "test-session-name", /*success=*/true);
  ASSERT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

  // Once the user finishes the initial setup, the service can actually start
  // configuring the data types. Just marking the initial setup as complete
  // isn't enough though, because setup is still considered in progress (we
  // haven't released the setup-in-progress handle).
  sync_service()->SetFirstSetupComplete();
  EXPECT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
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
  EXPECT_EQ(syncer::SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  DataTypeManager::ConfigureResult configure_result(
      DataTypeManager::OK, syncer::ModelTypeSet(syncer::SESSIONS));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->OnConfigureDone(configure_result);
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
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
                    syncer::ModelTypeSet(syncer::SESSIONS));
  sync_service()->Initialize();

  // Nothing is preventing Sync from starting, but it should be deferred so as
  // to now slow down browser startup.
  EXPECT_EQ(syncer::SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());

  // Wait for the deferred startup timer to expire. The Sync service will start
  // and initialize the engine.
  EXPECT_CALL(*sync_engine, Initialize(_));
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());

  // Once the engine calls back and says it's initialized, the DataTypeManager
  // will get configured, since initial setup is already done.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->OnEngineInitialized(
      syncer::ModelTypeSet(), syncer::WeakHandle<syncer::JsBackend>(),
      syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(), "test-guid",
      "test-session-name", /*success=*/true);
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURING));
  EXPECT_EQ(syncer::SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  DataTypeManager::ConfigureResult configure_result(
      DataTypeManager::OK, syncer::ModelTypeSet(syncer::SESSIONS));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  sync_service()->OnConfigureDone(configure_result);
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}

}  // namespace browser_sync
