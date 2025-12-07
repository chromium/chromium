// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "components/sync/test/fake_sync_engine.h"
#include "components/sync/test/fake_sync_engine_factory.h"
#include "components/sync/test/sync_client_mock.h"
#include "components/sync/test/sync_service_impl_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace syncer {

namespace {

constexpr char kEmail[] = "test_user@gmail.com";
constexpr char kTimeDeferredHistogram[] = "Sync.Startup.TimeDeferred2";

class MockSyncServiceObserver : public SyncServiceObserver {
 public:
  MockSyncServiceObserver() = default;

  MOCK_METHOD(void, OnStateChanged, (SyncService*), (override));
  MOCK_METHOD(void, OnSyncShutdown, (SyncService*), (override));
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

  signin::GaiaIdHash gaia_id_hash() {
    return signin::GaiaIdHash::FromGaiaId(
        sync_service_impl_bundle_.identity_test_env()
            ->identity_manager()
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .gaia);
  }

  void CreateSyncServiceWithControllers(
      DataTypeController::TypeVector controllers) {
    // Hold raw pointers to directly interact with the controllers.
    for (const auto& controller : controllers) {
      controller_map_[controller->type()] =
          static_cast<FakeDataTypeController*>(controller.get());
    }

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    ON_CALL(*sync_client, GetIdentityManager)
        .WillByDefault(Return(sync_service_impl_bundle_.identity_manager()));

    sync_service_ = std::make_unique<SyncServiceImpl>(
        sync_service_impl_bundle_.CreateBasicInitParams(
            std::move(sync_client)));
    sync_service_->Initialize(std::move(controllers));
  }

  void CreateSyncService(DataTypeSet registered_types = {BOOKMARKS}) {
    DataTypeController::TypeVector controllers;
    for (DataType type : registered_types) {
      controllers.push_back(std::make_unique<FakeDataTypeController>(type));
    }
    CreateSyncServiceWithControllers(std::move(controllers));
  }

  void SignInWithoutSyncConsent() {
    sync_service_impl_bundle_.identity_test_env()->MakePrimaryAccountAvailable(
        kEmail, signin::ConsentLevel::kSignin);
  }

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  void SignInWithSyncConsent() {
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

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  void SignInWithSyncConsentWithoutRefreshToken() {
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

  // Must only be called before CreateSyncService(), because it bypasses
  // SyncService/SyncUserSettings and uses SyncPrefs directly.
  void SetSyncFeatureEnabledPrefs() {
    CHECK(!sync_service_);

#if !BUILDFLAG(IS_CHROMEOS)
    sync_prefs_.SetInitialSyncFeatureSetupComplete();
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

  SyncPrefs* sync_prefs() { return &sync_prefs_; }

  SyncServiceImpl* sync_service() { return sync_service_.get(); }

  PrefService* pref_service() {
    return sync_service_impl_bundle_.pref_service();
  }

  FakeSyncEngineFactory* engine_factory() {
    return sync_service_impl_bundle_.engine_factory();
  }

  FakeSyncEngine* engine() { return engine_factory()->last_created_engine(); }

  FakeDataTypeController* get_controller(DataType type) {
    return controller_map_[type];
  }

  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  SyncServiceImplBundle sync_service_impl_bundle_;
  SyncPrefs sync_prefs_;
  std::unique_ptr<SyncServiceImpl> sync_service_;
  // The controllers are owned by `sync_service_`.
  std::map<DataType, raw_ptr<FakeDataTypeController, CtnExperimental>>
      controller_map_;
};

// ChromeOS does not support sign-in after startup
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncServiceImplStartupTest, StartFirstTime) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsInitialSyncFeatureSetupComplete());

  CreateSyncService();

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
  EXPECT_EQ(nullptr, engine());

  // Preferences should be back to defaults.
  EXPECT_EQ(base::Time(), sync_service()->GetLastSyncedTimeForDebugging());
  EXPECT_FALSE(sync_prefs()->IsInitialSyncFeatureSetupComplete());

  // Sign in and turn sync on, without marking the first setup as complete.
  SignInWithSyncConsent();
  std::unique_ptr<SyncSetupInProgressHandle> sync_blocker =
      sync_service()->GetSetupInProgressHandle();

  base::RunLoop().RunUntilIdle();

  // The engine can start, and engine initialization is immediate in this test,
  // so we bypass the INITIALIZING state.
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
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  // Sync-the-feature is still not active, but rather pending confirmation.
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());

  // Marking first setup complete will let SyncServiceImpl reconfigure the
  // DataTypeManager in full Sync-the-feature mode.
  sync_service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  // This should have fully enabled sync.
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(SyncServiceImplStartupTest, StartNoCredentials) {
  // The user is already signed in, but refresh tokens aren't loaded yet.
  SimulateRefreshTokensNotLoadedYet();
  SignInWithSyncConsentWithoutRefreshToken();
  SetSyncFeatureEnabledPrefs();

  CreateSyncService();
  FastForwardUntilNoTasksRemain();

  if (base::FeatureList::IsEnabled(kSyncDetermineAccountManagedStatus)) {
    // Without refresh tokens, SyncServiceImpl can't determine whether the
    // signed-in account is managed or not, so it'll remain in the INITIALIZING
    // state.
    EXPECT_EQ(SyncService::TransportState::INITIALIZING,
              sync_service()->GetTransportState());
  } else {
    // SyncServiceImpl should now be active.
    EXPECT_EQ(SyncService::TransportState::ACTIVE,
              sync_service()->GetTransportState());
  }
  // Either way, it shouldn't have an access token.
  EXPECT_TRUE(sync_service()->GetAccessTokenForTest().empty());
  // Note that SyncServiceImpl is not in an auth error state - no auth was
  // attempted, so no error.
}

TEST_F(SyncServiceImplStartupTest, WebSignoutBeforeInitialization) {
  // There is a primary account, but it's in a "web signout" aka sync-paused
  // state.
  SignInWithSyncConsent();
  SimulateWebSignout();
  SetSyncFeatureEnabledPrefs();

  CreateSyncService();

  // SyncServiceImpl should now be in the paused state.
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());
}

TEST_F(SyncServiceImplStartupTest, WebSignoutDuringDeferredStartup) {
  // There is a primary account. It is theoretically in the "web signout" aka
  // sync-paused error state, but the identity code hasn't detected that yet
  // (because auth errors are not persisted).
  base::HistogramTester histogram_tester;
  SignInWithSyncConsent();
  SetSyncFeatureEnabledPrefs();

  // Deferred startup is only possible if first sync completed earlier.
  engine_factory()->set_first_time_sync_configure_done(true);

  CreateSyncService();

  // There should be a deferred start task scheduled.
  ASSERT_EQ(SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());

  // Entering the sync-paused state should trigger a notification.
  // Note: Depending on the exact sequence of IdentityManager::Observer calls
  // (refresh token changed and/or auth error changed), there might be multiple
  // notifications.
  MockSyncServiceObserver observer;
  EXPECT_CALL(observer, OnStateChanged(sync_service()))
      .Times(testing::AtLeast(1))
      .WillRepeatedly([&]() {
        EXPECT_EQ(SyncService::TransportState::PAUSED,
                  sync_service()->GetTransportState());
      });

  // Now sign out on the web to enter the sync-paused state. Wait for the
  // deferred start task to run.
  sync_service()->AddObserver(&observer);
  SimulateWebSignout();
  sync_service()->RemoveObserver(&observer);
  FastForwardUntilNoTasksRemain();

  // SyncServiceImpl should now be in the paused state. The deferred task was
  // a no-op.
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());
  EXPECT_TRUE(histogram_tester.GetAllSamples(kTimeDeferredHistogram).empty());
}

TEST_F(SyncServiceImplStartupTest, WebSignoutAfterInitialization) {
  // This test has to wait for the access token request to complete, so disable
  // automatic issuing of tokens.
  DisableAutomaticIssueOfAccessTokens();

  SignInWithSyncConsent();
  SetSyncFeatureEnabledPrefs();

  CreateSyncService();

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
  SignInWithSyncConsent();
  SetSyncFeatureEnabledPrefs();

  // Prevent automatic (and successful) completion of engine initialization.
  engine_factory()->AllowFakeEngineInitCompletion(false);

  CreateSyncService();

  FastForwardUntilNoTasksRemain();
  // Simulate an auth error while downloading control types.
  engine()->TriggerInitializationCompletion(/*success=*/false);

  // Engine initialization failures puts the service into an unrecoverable error
  // state. It'll take either a browser restart or a full sign-out+sign-in to
  // get out of this.
  EXPECT_TRUE(sync_service()->HasUnrecoverableError());
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR}),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncServiceImplStartupTest, StartAshNoCredentials) {
  // We've never completed startup.
  ASSERT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));

  // On ChromeOS, the user is always immediately signed in, but a refresh token
  // isn't necessarily available yet.
  SimulateRefreshTokensNotLoadedYet();
  SignInWithSyncConsentWithoutRefreshToken();

  CreateSyncService();

  // Calling Initialize should cause the service to immediately create and
  // initialize the engine, and configure the DataTypeManager.
  base::RunLoop().RunUntilIdle();

  if (base::FeatureList::IsEnabled(kSyncDetermineAccountManagedStatus)) {
    // Without refresh tokens, SyncServiceImpl can't determine whether the
    // signed-in account is managed or not, so it'll remain in the INITIALIZING
    // state.
    EXPECT_EQ(SyncService::TransportState::INITIALIZING,
              sync_service()->GetTransportState());
  } else {
    // Sync should be considered active, even though there is no refresh token.
    EXPECT_EQ(SyncService::TransportState::ACTIVE,
              sync_service()->GetTransportState());
  }
  // FirstSetupComplete gets set automatically on Ash.
  EXPECT_TRUE(
      sync_service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
}

TEST_F(SyncServiceImplStartupTest, StartAshFirstTime) {
  // We've never completed Sync startup.
  ASSERT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));

  // There is already a signed-in user.
  SignInWithSyncConsent();

  // Sync should become active, even though IsInitialSyncFeatureSetupComplete
  // wasn't set yet.
  CreateSyncService();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
}
#endif

TEST_F(SyncServiceImplStartupTest, ResetSyncViaDashboard) {
  SetSyncFeatureEnabledPrefs();
  SignInWithSyncConsent();
  CreateSyncService();

  FastForwardUntilNoTasksRemain();
  ASSERT_TRUE(sync_service()->IsSyncFeatureActive());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  // Mimic sync reset via the https://chrome.google.com/data dashboard.
  // Sync-the-feature should be disabled. On desktop, the sync service will
  // immediately start up again in transport mode. On mobile the account is
  // removed and transport is disabled. InitialSyncFeatureSetupComplete is reset
  // on all platforms but Ash.
  sync_service()->OnActionableProtocolError(
      {.error_type = NOT_MY_BIRTHDAY, .action = DISABLE_SYNC_ON_CLIENT});
  base::RunLoop().RunUntilIdle();
  auto expected_transport_state_after_reset = SyncService::TransportState::
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
      DISABLED;
#else
      ACTIVE;
#endif

  EXPECT_EQ(expected_transport_state_after_reset,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(
      sync_service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  EXPECT_TRUE(
      sync_service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard());
#else   // BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(
      sync_service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Reset sync again while the sync service is already in transport mode. It
  // should immediately start up again in transport mode.
  sync_service()->OnActionableProtocolError(
      {.error_type = NOT_MY_BIRTHDAY, .action = DISABLE_SYNC_ON_CLIENT});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_transport_state_after_reset,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(sync_service()->GetActiveDataTypes().Has(BOOKMARKS));

  // On ChromeOS, test clearing the dashboard error, which should start
  // sync-the-feature and start BOOKMARKS.
  sync_service()->GetUserSettings()->ClearSyncFeatureDisabledViaDashboard();
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  EXPECT_TRUE(sync_service()->GetActiveDataTypes().Has(BOOKMARKS));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// ChromeOS does not support sign-in after startup.
#if !BUILDFLAG(IS_CHROMEOS)
// Verify that enabling sync honors existing values of data type preferences.
TEST_F(SyncServiceImplStartupTest, HonorsExistingDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  sync_prefs()->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/{UserSelectableType::kBookmarks});

  CreateSyncService();
  SignInWithSyncConsent();
  sync_service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  EXPECT_EQ(UserSelectableTypeSet({UserSelectableType::kBookmarks}),
            sync_service()->GetUserSettings()->GetSelectedTypes());
}
#endif

TEST_F(SyncServiceImplStartupTest, ManagedStartup) {
  // Sync was previously enabled, but a policy was set while Chrome wasn't
  // running.
  pref_service()->SetBoolean(prefs::internal::kSyncManaged, true);
  SetSyncFeatureEnabledPrefs();

  SignInWithSyncConsent();
  CreateSyncService();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            sync_service()->GetDisableReasons());
  // Service should not be started by Initialize() since it's managed.
  EXPECT_EQ(nullptr, engine());
}

TEST_F(SyncServiceImplStartupTest, SwitchManaged) {
  // Sync starts out fully set up and enabled.
  SetSyncFeatureEnabledPrefs();
  SignInWithSyncConsent();

  CreateSyncService();

  // Wait for deferred startup.
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::DisableReasonSet(),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());

  // The service should stop when switching to managed mode.
  pref_service()->SetBoolean(prefs::internal::kSyncManaged, true);
  // Give re-startup a chance to happen (it shouldn't!).
  base::RunLoop().RunUntilIdle();
  // Sync was disabled due to the policy.
  ASSERT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            sync_service()->GetDisableReasons());
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
  EXPECT_EQ(1, get_controller(BOOKMARKS)->model()->clear_metadata_count());

  // When switching back to unmanaged, Sync-the-transport should start up
  // automatically, which causes (re)creation of SyncEngine and
  // DataTypeManager.
  pref_service()->SetBoolean(prefs::internal::kSyncManaged, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_EQ(SyncService::DisableReasonSet(),
            sync_service()->GetDisableReasons());

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(
      sync_service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  // On ChromeOS Ash, sync-the-feature stays disabled even after the policy is
  // removed, for historic reasons. It is unclear if this behavior is optional,
  // because it is indistinguishable from the sync-reset-via-dashboard case.
  // It can be resolved by invoking ClearSyncFeatureDisabledViaDashboard().
  EXPECT_TRUE(
      sync_service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard());
#else
  EXPECT_FALSE(
      sync_service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
#endif  // BUILDFLAG(IS_CHROMEOS)

  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(sync_service()->IsSyncFeatureActive());
}

TEST_F(SyncServiceImplStartupTest, StartDownloadFailed) {
  // Prevent automatic (and successful) completion of engine initialization.
  engine_factory()->AllowFakeEngineInitCompletion(false);

  CreateSyncService();
  SignInWithSyncConsent();
  ASSERT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));

#if !BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(sync_prefs()->IsInitialSyncFeatureSetupComplete());
#endif  // !BUILDFLAG(IS_CHROMEOS)

  FastForwardUntilNoTasksRemain();

  // Simulate a failure while downloading control types.
  engine()->TriggerInitializationCompletion(/*success=*/false);

  std::unique_ptr<SyncSetupInProgressHandle> sync_blocker =
      sync_service()->GetSetupInProgressHandle();
  sync_blocker.reset();
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR}),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());
}

// ChromeOS does not support sign-in after startup.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncServiceImplStartupTest, FullStartupSequenceFirstTime) {
  // We've never completed startup.
  ASSERT_FALSE(sync_prefs()->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));

  CreateSyncService({SESSIONS});
  ASSERT_FALSE(sync_service()->CanSyncFeatureStart());

  // There is no signed-in user, so also nobody has decided that Sync should be
  // started.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            sync_service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            sync_service()->GetTransportState());

  // Sign in. Now Sync-the-transport can start. Since this was triggered by an
  // explicit user event, deferred startup is bypassed.
  // Sync-the-feature still doesn't start until the user says they want it.
  engine_factory()->AllowFakeEngineInitCompletion(false);
  SignInWithoutSyncConsent();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sync_service()->GetDisableReasons().empty());
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());
  EXPECT_FALSE(sync_service()->IsSyncFeatureEnabled());
  ASSERT_TRUE(engine());

  // Initiate Sync (the feature) setup before the engine initializes itself in
  // transport mode.
  SignInWithSyncConsent();
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
  sync_service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
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
  EXPECT_NE(nullptr, engine());

  // Finally, once the DataTypeManager says it's done with configuration, Sync
  // is actually fully up and running.
  get_controller(SESSIONS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(sync_service()->IsSyncFeatureActive());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(SyncServiceImplStartupTest, FullStartupSequenceNthTime) {
  // The user is already signed in and has completed Sync setup before.
  // Prevent engine initialization, to test TransportState::START_DEFERRED.
  // Prevent one model initialization, to test TransportState::CONFIGURING.
  SignInWithSyncConsent();
  SetSyncFeatureEnabledPrefs();

  // Deferred startup is only possible if first sync completed earlier.
  engine_factory()->set_first_time_sync_configure_done(true);
  engine_factory()->AllowFakeEngineInitCompletion(false);
  auto controller = std::make_unique<FakeDataTypeController>(SESSIONS);
  controller->model()->EnableManualModelStart();
  DataTypeController::TypeVector controllers;
  controllers.push_back(std::move(controller));
  CreateSyncServiceWithControllers(std::move(controllers));

  // Nothing is preventing Sync from starting, but it should be deferred so as
  // to not slow down browser startup.
  ASSERT_TRUE(sync_service()->CanSyncFeatureStart());
  EXPECT_EQ(SyncService::TransportState::START_DEFERRED,
            sync_service()->GetTransportState());
  EXPECT_EQ(nullptr, engine());

  // Cause the deferred startup timer to expire.
  FastForwardUntilNoTasksRemain();

  // The Sync service should start initializing the engine.
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());
  EXPECT_NE(nullptr, engine());

  // Allow engine initialization to finish.
  engine()->TriggerInitializationCompletion(/*success=*/true);

  // The DataTypeManager should start configuring, since initial setup is
  // already done.
  EXPECT_EQ(SyncService::TransportState::CONFIGURING,
            sync_service()->GetTransportState());
  EXPECT_TRUE(engine());

  // Finish model initialization.
  get_controller(SESSIONS)->model()->SimulateModelStartFinished();

  // Sync is fully up and running.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());
  EXPECT_TRUE(engine());
}

TEST_F(SyncServiceImplStartupTest, DeferredStartInterruptedByDataType) {
  base::HistogramTester histogram_tester;
  SetSyncFeatureEnabledPrefs();

  // Deferred startup is only possible if first sync completed earlier.
  engine_factory()->set_first_time_sync_configure_done(true);

  SignInWithSyncConsent();
  CreateSyncService();

  // A deferred start task should be scheduled.
  EXPECT_EQ(sync_service()->GetTransportState(),
            syncer::SyncService::TransportState::START_DEFERRED);
  EXPECT_TRUE(histogram_tester.GetAllSamples(kTimeDeferredHistogram).empty());

  // A data type requests immediate initialization.
  sync_service()->OnDataTypeRequestsSyncStartup(BOOKMARKS);
  base::RunLoop().RunUntilIdle();

  // Deferral should be interrupted and sync started immediately. The premature
  // start should be recorded in metrics.
  EXPECT_EQ(sync_service()->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(1u, histogram_tester.GetAllSamples(kTimeDeferredHistogram).size());

  // There's still a deferred task scheduled. Let it run.
  FastForwardUntilNoTasksRemain();

  // The task should be a no-op.
  EXPECT_EQ(sync_service()->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_EQ(1u, histogram_tester.GetAllSamples(kTimeDeferredHistogram).size());
}

// ChromeOS does not support sign-in after startup.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SyncServiceImplStartupTest, UserTriggeredStartIsNotDeferredStart) {
  // Signed-out at first.
  base::HistogramTester histogram_tester;
  CreateSyncService();

  // Sign-in quickly, before the usual delay of a deferred startup. This can
  // happen during FRE.
  SignInWithSyncConsent();
  sync_service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  FastForwardUntilNoTasksRemain();

  // This should not be recorded as a deferred startup.
  EXPECT_EQ(sync_service()->GetTransportState(),
            syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(histogram_tester.GetAllSamples(kTimeDeferredHistogram).empty());
}
#endif

TEST_F(SyncServiceImplStartupTest,
       ShouldClearMetadataForAlreadyDisabledTypesBeforeConfigurationDone) {
  SetSyncFeatureEnabledPrefs();
  // Simulate types disabled during previous run.
  sync_prefs()->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/
      {UserSelectableType::kBookmarks, UserSelectableType::kReadingList},
      /*selected_types=*/{UserSelectableType::kBookmarks});

  SignInWithSyncConsent();

  CreateSyncService(/*registered_types=*/{BOOKMARKS, READING_LIST});

  // Metadata was cleared for disabled types ...
  EXPECT_EQ(1, get_controller(READING_LIST)->model()->clear_metadata_count());
  // ... but not for the ones not disabled.
  EXPECT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(SyncServiceImplStartupTest,
       ShouldClearMetadataForTypesDisabledBeforeInitCompletion) {
  SignInWithSyncConsent();
  SetSyncFeatureEnabledPrefs();

  engine_factory()->AllowFakeEngineInitCompletion(false);

  CreateSyncService(/*registered_types=*/{BOOKMARKS, READING_LIST});
  FastForwardUntilNoTasksRemain();

  // Simulate opening sync settings before engine init is over.
  std::unique_ptr<SyncSetupInProgressHandle> setup_in_progress_handle =
      sync_service()->GetSetupInProgressHandle();
  // Disable READING_LIST type before engine init is over.
  sync_prefs()->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/{UserSelectableType::kBookmarks});
  setup_in_progress_handle.reset();

  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_TRUE(sync_service()->IsEngineInitialized());
  // Metadata was cleared for disabled types ...
  EXPECT_EQ(1, get_controller(READING_LIST)->model()->clear_metadata_count());
  // ... but not for the ones not disabled.
  EXPECT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());
}

TEST_F(SyncServiceImplStartupTest,
       ShouldClearMetadataForTypesDisabledWhileInit) {
  SignInWithSyncConsent();
  SetSyncFeatureEnabledPrefs();

  engine_factory()->AllowFakeEngineInitCompletion(false);

  CreateSyncService(/*registered_types=*/{BOOKMARKS, READING_LIST});
  FastForwardUntilNoTasksRemain();

  // Simulate opening sync settings before engine init is over.
  std::unique_ptr<SyncSetupInProgressHandle> setup_in_progress_handle =
      sync_service()->GetSetupInProgressHandle();
  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_TRUE(sync_service()->IsEngineInitialized());

  // Disable READING_LIST type.
  sync_prefs()->SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/{UserSelectableType::kBookmarks});

  // This should trigger reconfiguration.
  setup_in_progress_handle.reset();

  // Metadata was cleared for disabled types ...
  EXPECT_EQ(1, get_controller(READING_LIST)->model()->clear_metadata_count());
  // ... but not for the ones not disabled.
  EXPECT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());
}

class SyncServiceImplStartupWithDetermineAccountTypeTest
    : public SyncServiceImplStartupTest {
 public:
  SyncServiceImplStartupWithDetermineAccountTypeTest() {
    // Disable the timeout for determining the managed status, since otherwise
    // the FastForward*() calls would trigger the account status fetcher to
    // time out (which would then unblock the sync startup, and make these tests
    // pointless).
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kSyncDetermineAccountManagedStatus,
        {{kSyncDetermineAccountManagedStatusTimeout.name, "inf"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SyncServiceImplStartupWithDetermineAccountTypeTest,
       StartupWithConsumerAccount) {
  // Set up a consumer account, for which the managed status can be determined
  // synchronously.
  SignInWithSyncConsent();

  SetSyncFeatureEnabledPrefs();
  CreateSyncService();
  FastForwardUntilNoTasksRemain();

  // The service should become active without further info, i.e. without the
  // account type being determined explicitly.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  // Ensure the correct info arrived at the controller.
  EXPECT_EQ(get_controller(BOOKMARKS)
                ->last_configure_context()
                .account_managed_status,
            signin::AccountManagedStatusFinderOutcome::kConsumerGmail);
}

TEST_F(SyncServiceImplStartupWithDetermineAccountTypeTest,
       StartupWithManagedAccount) {
  // Set up a managed account, for which the managed status is determined
  // asynchronously.
  AccountInfo account_info =
      sync_service_impl_bundle_.identity_test_env()
          ->MakePrimaryAccountAvailable("test_user@managed.com",
                                        signin::ConsentLevel::kSync);
  ASSERT_EQ(account_info.IsManaged(), signin::Tribool::kUnknown);

  SetSyncFeatureEnabledPrefs();
  CreateSyncService();
  FastForwardUntilNoTasksRemain();

  // The service should remain waiting for the account type to be determined.
  EXPECT_EQ(SyncService::TransportState::INITIALIZING,
            sync_service()->GetTransportState());

  // Now provide the information that this is a managed account.
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain("managed.com").Build();
  sync_service_impl_bundle_.identity_test_env()->UpdateAccountInfoForAccount(
      account_info);
  ASSERT_EQ(account_info.IsManaged(), signin::Tribool::kTrue);

  // The service should now become active.
  FastForwardUntilNoTasksRemain();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            sync_service()->GetTransportState());

  // Ensure the correct info arrived at the controller.
  EXPECT_EQ(get_controller(BOOKMARKS)
                ->last_configure_context()
                .account_managed_status,
            signin::AccountManagedStatusFinderOutcome::kEnterprise);
}

}  // namespace syncer
