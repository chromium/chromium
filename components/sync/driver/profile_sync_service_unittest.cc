// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/profile_sync_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_demographics.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/profile_sync_service_bundle.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/engine/fake_sync_engine.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info_values.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace syncer {

namespace {

// Age of a user that is old enough to provide demographics when now time is
// |kNowTimeInStringFormat|.
constexpr int kOldEnoughForDemographicsUserBirthYear = 1983;

constexpr char kTestUser[] = "test_user@gmail.com";

// Now time in string format.
constexpr char kNowTimeInStringFormat[] = "23 Mar 2019 16:00:00 UDT";

class FakeDataTypeManager : public DataTypeManager {
 public:
  using ConfigureCalled = base::RepeatingCallback<void(ConfigureReason)>;

  explicit FakeDataTypeManager(const ConfigureCalled& configure_called)
      : configure_called_(configure_called), state_(STOPPED) {}

  ~FakeDataTypeManager() override {}

  void Configure(ModelTypeSet desired_types,
                 const ConfigureContext& context) override {
    state_ = CONFIGURED;
    DCHECK(!configure_called_.is_null());
    configure_called_.Run(context.reason);
  }

  void DataTypePreconditionChanged(ModelType type) override {}
  void ResetDataTypeErrors() override {}
  void PurgeForMigration(ModelTypeSet undesired_types) override {}
  void Stop(ShutdownReason reason) override {}
  ModelTypeSet GetActiveDataTypes() const override { return ModelTypeSet(); }
  bool IsNigoriEnabled() const override { return true; }
  State state() const override { return state_; }

 private:
  ConfigureCalled configure_called_;
  State state_;
};

ACTION_P(ReturnNewFakeDataTypeManager, configure_called) {
  return std::make_unique<FakeDataTypeManager>(configure_called);
}

class TestSyncServiceObserver : public SyncServiceObserver {
 public:
  TestSyncServiceObserver()
      : setup_in_progress_(false), auth_error_(GoogleServiceAuthError()) {}

  void OnStateChanged(SyncService* sync) override {
    setup_in_progress_ = sync->IsSetupInProgress();
    auth_error_ = sync->GetAuthError();
  }

  bool setup_in_progress() const { return setup_in_progress_; }
  GoogleServiceAuthError auth_error() const { return auth_error_; }

 private:
  bool setup_in_progress_;
  GoogleServiceAuthError auth_error_;
};

// A variant of the FakeSyncEngine that won't automatically call back when asked
// to initialize. Allows us to test things that could happen while backend init
// is in progress.
class FakeSyncEngineNoReturn : public FakeSyncEngine {
  void Initialize(InitParams params) override {}
};

// FakeSyncEngine that stores the account ID passed into Initialize(), and
// optionally also whether InvalidateCredentials was called.
class FakeSyncEngineCollectCredentials : public FakeSyncEngine {
 public:
  explicit FakeSyncEngineCollectCredentials(
      CoreAccountId* init_account_id,
      const base::RepeatingClosure& invalidate_credentials_callback)
      : init_account_id_(init_account_id),
        invalidate_credentials_callback_(invalidate_credentials_callback) {}

  void Initialize(InitParams params) override {
    *init_account_id_ = params.authenticated_account_id;
    FakeSyncEngine::Initialize(std::move(params));
  }

  void InvalidateCredentials() override {
    if (invalidate_credentials_callback_) {
      invalidate_credentials_callback_.Run();
    }
    FakeSyncEngine::InvalidateCredentials();
  }

 private:
  CoreAccountId* init_account_id_;
  base::RepeatingClosure invalidate_credentials_callback_;
};

ACTION(ReturnNewFakeSyncEngine) {
  return std::make_unique<FakeSyncEngine>();
}

ACTION(ReturnNewFakeSyncEngineNoReturn) {
  return std::make_unique<FakeSyncEngineNoReturn>();
}

// A test harness that uses a real ProfileSyncService and in most cases a
// FakeSyncEngine.
//
// This is useful if we want to test the ProfileSyncService and don't care about
// testing the SyncEngine.
class ProfileSyncServiceTest : public ::testing::Test {
 protected:
  ProfileSyncServiceTest() {}
  ~ProfileSyncServiceTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "0");
  }

  void TearDown() override {
    // Kill the service before the profile.
    ShutdownAndDeleteService();
  }

  void SignIn() { identity_test_env()->MakePrimaryAccountAvailable(kTestUser); }

  void CreateService(ProfileSyncService::StartBehavior behavior) {
    DCHECK(!service_);

    DataTypeController::TypeVector controllers;
    controllers.push_back(std::make_unique<FakeDataTypeController>(BOOKMARKS));

    std::unique_ptr<SyncClientMock> sync_client =
        profile_sync_service_bundle_.CreateSyncClientMock();
    ON_CALL(*sync_client, CreateDataTypeControllers(_))
        .WillByDefault(Return(ByMove(std::move(controllers))));

    service_ = std::make_unique<ProfileSyncService>(
        profile_sync_service_bundle_.CreateBasicInitParams(
            behavior, std::move(sync_client)));

    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _))
        .WillByDefault(ReturnNewFakeSyncEngine());
    ON_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
        .WillByDefault(
            ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  }

  void CreateServiceWithLocalSyncBackend() {
    DCHECK(!service_);

    DataTypeController::TypeVector controllers;
    controllers.push_back(std::make_unique<FakeDataTypeController>(BOOKMARKS));

    std::unique_ptr<SyncClientMock> sync_client =
        profile_sync_service_bundle_.CreateSyncClientMock();
    ON_CALL(*sync_client, CreateDataTypeControllers(_))
        .WillByDefault(Return(ByMove(std::move(controllers))));

    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(
            ProfileSyncService::AUTO_START, std::move(sync_client));

    prefs()->SetBoolean(prefs::kEnableLocalSyncBackend, true);
    init_params.identity_manager = nullptr;

    service_ = std::make_unique<ProfileSyncService>(std::move(init_params));

    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _))
        .WillByDefault(ReturnNewFakeSyncEngine());
    ON_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
        .WillByDefault(
            ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  }

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void InitializeForNthSync() {
    // Set first sync time before initialize to simulate a complete sync setup.
    SyncPrefs sync_prefs(prefs());
    sync_prefs.SetLastSyncedTime(base::Time::Now());
    sync_prefs.SetSyncRequested(true);
    sync_prefs.SetSelectedTypes(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/UserSelectableTypeSet::All());
    sync_prefs.SetFirstSetupComplete();
    service_->Initialize();
  }

  void InitializeForFirstSync() { service_->Initialize(); }

  void TriggerPassphraseRequired() {
    service_->GetEncryptionObserverForTest()->OnPassphraseRequired(
        REASON_DECRYPTION, KeyDerivationParams::CreateForPbkdf2(),
        sync_pb::EncryptedData());
  }

  void TriggerDataTypeStartRequest() {
    service_->OnDataTypeRequestsSyncStartup(BOOKMARKS);
  }

  void OnConfigureCalled(ConfigureReason configure_reason) {
    DataTypeManager::ConfigureResult result;
    result.status = DataTypeManager::OK;
    service()->OnConfigureDone(result);
  }

  FakeDataTypeManager::ConfigureCalled GetDefaultConfigureCalledCallback() {
    return base::Bind(&ProfileSyncServiceTest::OnConfigureCalled,
                      base::Unretained(this));
  }

  FakeDataTypeManager::ConfigureCalled GetRecordingConfigureCalledCallback(
      ConfigureReason* reason_dest) {
    return base::BindLambdaForTesting(
        [reason_dest](ConfigureReason reason) { *reason_dest = reason; });
  }

  invalidation::ProfileIdentityProvider* identity_provider() {
    return profile_sync_service_bundle_.identity_provider();
  }

  signin::IdentityManager* identity_manager() {
    return profile_sync_service_bundle_.identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return profile_sync_service_bundle_.identity_test_env();
  }

  ProfileSyncService* service() { return service_.get(); }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_sync_service_bundle_.pref_service();
  }

  SyncApiComponentFactoryMock* component_factory() {
    return profile_sync_service_bundle_.component_factory();
  }

  void SetDemographics(int birth_year,
                       metrics::UserDemographicsProto_Gender gender) {
    base::DictionaryValue dict;
    dict.SetIntPath(prefs::kSyncDemographics_BirthYearPath, birth_year);
    dict.SetIntPath(prefs::kSyncDemographics_GenderPath,
                    static_cast<int>(gender));
    prefs()->Set(prefs::kSyncDemographics, dict);
  }

  static bool HasBirthYearDemographic(const PrefService* pref_service) {
    return pref_service->HasPrefPath(prefs::kSyncDemographics) &&
           pref_service->GetDictionary(prefs::kSyncDemographics)
               ->FindIntPath(prefs::kSyncDemographics_BirthYearPath);
  }

  static bool HasGenderDemographic(const PrefService* pref_service) {
    return pref_service->HasPrefPath(prefs::kSyncDemographics) &&
           pref_service->GetDictionary(prefs::kSyncDemographics)
               ->FindIntPath(prefs::kSyncDemographics_GenderPath);
  }

  static bool HasBirthYearOffset(const PrefService* pref_service) {
    return pref_service->HasPrefPath(prefs::kSyncDemographicsBirthYearOffset);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ProfileSyncServiceBundle profile_sync_service_bundle_;
  std::unique_ptr<ProfileSyncService> service_;
};

// Gets the now time used for testing user demographics.
base::Time GetNowTime() {
  base::Time now;
  bool result = base::Time::FromString(kNowTimeInStringFormat, &now);
  DCHECK(result);
  return now;
}

// Verify that the server URLs are sane.
TEST_F(ProfileSyncServiceTest, InitialState) {
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  const std::string& url = service()->GetSyncServiceUrlForDebugging().spec();
  EXPECT_TRUE(url == internal::kSyncServerUrl ||
              url == internal::kSyncDevServerUrl);
}

TEST_F(ProfileSyncServiceTest, SuccessfulInitialization) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(ReturnNewFakeSyncEngine());
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

TEST_F(ProfileSyncServiceTest, SuccessfulLocalBackendInitialization) {
  CreateServiceWithLocalSyncBackend();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(ReturnNewFakeSyncEngine());
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Verify that an initialization where first setup is not complete does not
// start up Sync-the-feature.
TEST_F(ProfileSyncServiceTest, NeedsConfirmation) {
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);

  SyncPrefs sync_prefs(prefs());
  base::Time now = base::Time::Now();
  sync_prefs.SetLastSyncedTime(now);
  sync_prefs.SetSyncRequested(true);
  sync_prefs.SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());
  service()->Initialize();

  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());

  // Sync should immediately start up in transport mode.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // The last sync time shouldn't be cleared.
  // TODO(zea): figure out a way to check that the directory itself wasn't
  // cleared.
  EXPECT_EQ(now, sync_prefs.GetLastSyncedTime());
}

// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(ProfileSyncServiceTest, SetupInProgress) {
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForFirstSync();

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  auto sync_blocker = service()->GetSetupInProgressHandle();
  EXPECT_TRUE(observer.setup_in_progress());
  sync_blocker.reset();
  EXPECT_FALSE(observer.setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that disable by enterprise policy works.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInit) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

// This test exercises sign-in after startup, which isn't supported on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInitThenPolicyRemoved) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DISABLE_REASON_ENTERPRISE_POLICY |
                SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Remove the policy. Now only missing sign-in is preventing startup.
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  EXPECT_EQ(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Once we mark first setup complete again (it was cleared by the policy) and
  // sign in, sync starts up.
  service()->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  SignIn();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}
#endif  // !defined(OS_CHROMEOS)

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyAfterInit) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  EXPECT_EQ(SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

// Exercises the ProfileSyncService's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  CreateService(ProfileSyncService::AUTO_START);
  ON_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillByDefault(ReturnNewFakeSyncEngineNoReturn());

  SignIn();
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  ShutdownAndDeleteService();
}

// Test SetSyncRequested(false) before we've initialized the backend.
TEST_F(ProfileSyncServiceTest, EarlyRequestStop) {
  CreateService(ProfileSyncService::AUTO_START);
  // Set up a fake sync engine that will not immediately finish initialization.
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(ReturnNewFakeSyncEngineNoReturn());
  SignIn();
  InitializeForNthSync();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // Request stop. This should immediately restart the service in standalone
  // transport mode.
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(ReturnNewFakeSyncEngine());
  service()->GetUserSettings()->SetSyncRequested(false);
  EXPECT_EQ(SyncService::DISABLE_REASON_USER_CHOICE,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // Request start. Now Sync-the-feature should start again.
  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
}

// Test SetSyncRequested(false) after we've initialized the backend.
TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  InitializeForNthSync();

  SyncPrefs sync_prefs(prefs());

  ASSERT_TRUE(sync_prefs.IsSyncRequested());
  ASSERT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(service()->IsSyncFeatureActive());
  ASSERT_TRUE(service()->IsSyncFeatureEnabled());

  testing::Mock::VerifyAndClearExpectations(component_factory());

  service()->GetUserSettings()->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs.IsSyncRequested());
  EXPECT_EQ(SyncService::DISABLE_REASON_USER_CHOICE,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs.IsSyncRequested());
  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out" and policy.
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, EnableSyncSignOutAndChangeAccount) {
  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  InitializeForNthSync();

  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(),
            identity_provider()->GetActiveAccountId());

  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);
  account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  // Wait for PSS to be notified that the primary account has gone away.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::DISABLE_REASON_NOT_SIGNED_IN,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_EQ(CoreAccountId(), identity_provider()->GetActiveAccountId());

  identity_test_env()->MakePrimaryAccountAvailable("new_user@gmail.com");
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(),
            identity_provider()->GetActiveAccountId());
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  CreateService(ProfileSyncService::AUTO_START);

  SignIn();
  InitializeForNthSync();

  // Initial status.
  SyncTokenStatus token_status = service()->GetSyncTokenStatusForDebugging();
  ASSERT_EQ(CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  ASSERT_TRUE(token_status.connection_status_update_time.is_null());
  ASSERT_FALSE(token_status.token_request_time.is_null());
  ASSERT_TRUE(token_status.token_response_time.is_null());
  ASSERT_FALSE(token_status.has_token);

  // The token request will take the form of a posted task.  Run it.
  base::RunLoop().RunUntilIdle();

  // Now we should have an access token.
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_TRUE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_response_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_TRUE(token_status.next_token_request_time.is_null());
  EXPECT_TRUE(token_status.has_token);

  // Simulate an auth error.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // This should get reflected in the status, and we should have dropped the
  // invalid access token.
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_EQ(CONNECTION_AUTH_ERROR, token_status.connection_status);
  EXPECT_FALSE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_response_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_FALSE(token_status.next_token_request_time.is_null());
  EXPECT_FALSE(token_status.has_token);

  // Simulate successful connection.
  service()->OnConnectionStatusChange(CONNECTION_OK);
  token_status = service()->GetSyncTokenStatusForDebugging();
  EXPECT_EQ(CONNECTION_OK, token_status.connection_status);
}

TEST_F(ProfileSyncServiceTest, RevokeAccessTokenFromTokenService) {
  CoreAccountId init_account_id;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_account_id, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId();

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_account_id);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());

  AccountInfo secondary_account_info =
      identity_test_env()->MakeAccountAvailable("test_user2@gmail.com");
  identity_test_env()->RemoveRefreshTokenForAccount(
      secondary_account_info.account_id);
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}

// Checks that CREDENTIALS_REJECTED_BY_CLIENT resets the access token and stops
// Sync. Regression test for https://crbug.com/824791.
TEST_F(ProfileSyncServiceTest, CredentialsRejectedByClient_StopSync) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(switches::kStopSyncInPausedState);

  CoreAccountId init_account_id;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_account_id, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId();

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_account_id);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), service()->GetAuthError());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.auth_error());

  // Simulate the credentials getting locally rejected by the client by setting
  // the refresh token to a special invalid value.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  const GoogleServiceAuthError rejected_by_client =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  ASSERT_EQ(rejected_by_client,
            identity_test_env()
                ->identity_manager()
                ->GetErrorStateOfRefreshTokenForAccount(primary_account_id));
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());

  // The observer should have been notified of the auth error state.
  EXPECT_EQ(rejected_by_client, observer.auth_error());
  // The Sync engine should have been shut down.
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_TRUE(service()->HasDisableReason(SyncService::DISABLE_REASON_PAUSED));

  service()->RemoveObserver(&observer);
}

TEST_F(ProfileSyncServiceTest, CredentialsRejectedByClient_DoNotStopSync) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(switches::kStopSyncInPausedState);

  CoreAccountId init_account_id;

  bool invalidate_credentials_called = false;
  base::RepeatingClosure invalidate_credentials_callback =
      base::BindRepeating([](bool* called) { *called = true; },
                          base::Unretained(&invalidate_credentials_called));

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_account_id, invalidate_credentials_callback))));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId();

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_account_id);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), service()->GetAuthError());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.auth_error());

  // Simulate the credentials getting locally rejected by the client by setting
  // the refresh token to a special invalid value.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  const GoogleServiceAuthError rejected_by_client =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  ASSERT_EQ(rejected_by_client,
            identity_test_env()
                ->identity_manager()
                ->GetErrorStateOfRefreshTokenForAccount(primary_account_id));
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
  EXPECT_TRUE(invalidate_credentials_called);

  // The observer should have been notified of the auth error state.
  EXPECT_EQ(rejected_by_client, observer.auth_error());
  // The Sync engine should still be running.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// CrOS does not support signout.
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, SignOutRevokeAccessToken) {
  CoreAccountId init_account_id;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_account_id, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId();

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_account_id);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  auto* account_mutator = identity_manager()->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

// Verify that prefs are cleared on sign-out.
TEST_F(ProfileSyncServiceTest, ClearDataOnSignOut) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  base::Time last_synced_time = service()->GetLastSyncedTimeForDebugging();
  ASSERT_LT(base::Time::Now() - last_synced_time,
            base::TimeDelta::FromMinutes(1));

  // Set demographic prefs that are normally fetched from server when syncing.
  SetDemographics(kOldEnoughForDemographicsUserBirthYear,
                  metrics::UserDemographicsProto_Gender_GENDER_FEMALE);

  // Set the birth year offset pref that would be normally set when calling
  // SyncPrefs::GetUserNoisedBirthYearAndGender().
  prefs()->SetInteger(prefs::kSyncDemographicsBirthYearOffset, 2);

  // Verify that the demographics prefs exist (i.e., that the test is set up).
  ASSERT_TRUE(HasBirthYearDemographic(prefs()));
  ASSERT_TRUE(HasGenderDemographic(prefs()));
  ASSERT_TRUE(HasBirthYearOffset(prefs()));

  // Sign out.
  service()->StopAndClear();

  // Even though Sync-the-feature is disabled, Sync-the-transport should still
  // be running, and should have updated the last synced time.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  EXPECT_NE(service()->GetLastSyncedTimeForDebugging(), last_synced_time);

  // Check that the demographic prefs are cleared.
  EXPECT_FALSE(prefs()->HasPrefPath(prefs::kSyncDemographics));
  EXPECT_FALSE(HasBirthYearDemographic(prefs()));
  EXPECT_FALSE(HasGenderDemographic(prefs()));

  // Verify that the random offset is preserved. If the user signs in again,
  // we don't want them to start reporting a different randomized birth year
  // as this could narrow down or ever reveal their true birth year.
  EXPECT_TRUE(HasBirthYearOffset(prefs()));
}

// Verify that demographic prefs are cleared when the service is initializing
// and account is signed out.
TEST_F(ProfileSyncServiceTest, ClearDemographicsOnInitializeWhenSignedOut) {
  // Set demographic prefs that are leftovers from previous sync. We can imagine
  // that due to some crash, sync service did not clear demographics when
  // account was signed out.
  SetDemographics(kOldEnoughForDemographicsUserBirthYear,
                  metrics::UserDemographicsProto_Gender_GENDER_FEMALE);

  // Set the birth year offset pref that would be normally set when calling
  // SyncPrefs::GetUserNoisedBirthYearAndGender().
  prefs()->SetInteger(prefs::kSyncDemographicsBirthYearOffset, 2);

  // Verify that the demographics prefs exist (i.e., that the test is set up).
  ASSERT_TRUE(HasBirthYearDemographic(prefs()));
  ASSERT_TRUE(HasGenderDemographic(prefs()));
  ASSERT_TRUE(HasBirthYearOffset(prefs()));

  // Don't sign-in before creating the service.
  CreateService(ProfileSyncService::AUTO_START);
  // Initialize when signed out to trigger clearing of demographic prefs.
  InitializeForNthSync();

  // Verify that the demographic prefs are cleared.
  EXPECT_FALSE(prefs()->HasPrefPath(prefs::kSyncDemographics));
  EXPECT_FALSE(HasBirthYearDemographic(prefs()));
  EXPECT_FALSE(HasGenderDemographic(prefs()));

  // Verify that the random offset is preserved. If the user signs in again,
  // we don't want them to start reporting a different randomized birth year
  // as this could narrow down or ever reveal their true birth year.
  EXPECT_TRUE(HasBirthYearOffset(prefs()));
}

TEST_F(ProfileSyncServiceTest, CancelSyncAfterSignOut) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  base::Time last_synced_time = service()->GetLastSyncedTimeForDebugging();
  ASSERT_LT(base::Time::Now() - last_synced_time,
            base::TimeDelta::FromMinutes(1));

  // Disable sync.
  service()->StopAndClear();
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());

  // Calling StopAndClear while already stopped should not crash. This may
  // (under some circumstances) happen when the user enables sync again but hits
  // the cancel button at the end of the process.
  ASSERT_FALSE(service()->GetUserSettings()->IsSyncRequested());
  service()->StopAndClear();
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}

// Verify that credential errors get returned from GetAuthError().
TEST_F(ProfileSyncServiceTest, CredentialErrorReturned) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  CoreAccountId init_account_id;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_account_id, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId();

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_account_id);

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for ProfileSyncService to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            observer.auth_error().state());
  // The overall state should remain ACTIVE.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that credential errors get cleared when a new token is fetched
// successfully.
TEST_F(ProfileSyncServiceTest, CredentialErrorClearsOnNewToken) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  CoreAccountId init_account_id;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_account_id, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId();

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_account_id);

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for ProfileSyncService to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Wait for ProfileSyncService to be notified of the changed credentials and
  // send a new access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  // The overall state should remain ACTIVE.
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Now emulate Chrome receiving a new, valid LST.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "this one works", base::Time::Now() + base::TimeDelta::FromDays(10));

  // Check that sync auth error state cleared.
  EXPECT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::NONE, observer.auth_error().state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that the disable sync flag disables sync.
TEST_F(ProfileSyncServiceTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  EXPECT_FALSE(switches::IsSyncAllowedByFlag());
}

// Verify that no disable sync flag enables sync.
TEST_F(ProfileSyncServiceTest, NoDisableSyncFlag) {
  EXPECT_TRUE(switches::IsSyncAllowedByFlag());
}

// Test that the passphrase prompt due to version change logic gets triggered
// on a datatype type requesting startup, but only happens once.
TEST_F(ProfileSyncServiceTest, PassphrasePromptDueToVersion) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();

  SyncPrefs sync_prefs(prefs());
  ASSERT_EQ(PRODUCT_VERSION, sync_prefs.GetLastRunVersion());

  sync_prefs.SetPassphrasePrompted(true);

  // Until a datatype requests startup while a passphrase is required the
  // passphrase prompt bit should remain set.
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());
  TriggerPassphraseRequired();
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());

  // Because the last version was unset, this run should be treated as a new
  // version and force a prompt.
  TriggerDataTypeStartRequest();
  EXPECT_FALSE(sync_prefs.IsPassphrasePrompted());

  // At this point further datatype startup request should have no effect.
  sync_prefs.SetPassphrasePrompted(true);
  TriggerDataTypeStartRequest();
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());
}

// Test that when ProfileSyncService receives actionable error
// RESET_LOCAL_SYNC_DATA it restarts sync.
TEST_F(ProfileSyncServiceTest, ResetSyncData) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  // Backend should get initialized two times: once during initialization and
  // once when handling actionable error.
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _))
      .WillOnce(ReturnNewFakeSyncEngine())
      .WillOnce(ReturnNewFakeSyncEngine());

  InitializeForNthSync();

  SyncProtocolError client_cmd;
  client_cmd.action = RESET_LOCAL_SYNC_DATA;
  service()->OnActionableError(client_cmd);
}

// Test that when ProfileSyncService receives actionable error
// DISABLE_SYNC_ON_CLIENT it disables sync and signs out.
TEST_F(ProfileSyncServiceTest, DisableSyncOnClient) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_LT(base::Time::Now() - service()->GetLastSyncedTimeForDebugging(),
            base::TimeDelta::FromMinutes(1));

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableError(client_cmd);

#if defined(OS_CHROMEOS)
  // ChromeOS does not support signout.
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(SyncService::DISABLE_REASON_USER_CHOICE,
            service()->GetDisableReasons());
  // Since ChromeOS doesn't support signout and so the account is still there
  // and available, Sync will restart in standalone transport mode.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
#else
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(SyncService::DISABLE_REASON_NOT_SIGNED_IN |
                SyncService::DISABLE_REASON_USER_CHOICE,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetLastSyncedTimeForDebugging().is_null());
#endif

  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
}

// Verify a that local sync mode resumes after the policy is lifted.
TEST_F(ProfileSyncServiceTest, LocalBackendDisabledByPolicy) {
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  EXPECT_EQ(SyncService::DISABLE_REASON_ENTERPRISE_POLICY,
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Note: If standalone transport is enabled, then setting kSyncManaged to
  // false will immediately start up the engine. Otherwise, the RequestStart
  // call below will trigger it.
  prefs()->SetManagedPref(prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));

  service()->GetUserSettings()->SetSyncRequested(true);
  EXPECT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Test ConfigureDataTypeManagerReason on First and Nth start.
TEST_F(ProfileSyncServiceTest, ConfigureDataTypeManagerReason) {
  const DataTypeManager::ConfigureResult configure_result(DataTypeManager::OK,
                                                          ModelTypeSet());
  ConfigureReason configure_reason = CONFIGURE_REASON_UNKNOWN;

  SignIn();

  // First sync.
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForFirstSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(component_factory()));
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT, configure_reason);
  service()->OnConfigureDone(configure_result);

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION, configure_reason);
  service()->OnConfigureDone(configure_result);
  ShutdownAndDeleteService();

  // Nth sync.
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(component_factory()));
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  service()->OnConfigureDone(configure_result);

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION, configure_reason);
  service()->OnConfigureDone(configure_result);
  ShutdownAndDeleteService();
}

// Test whether sync service provides user demographics when sync is enabled.
TEST_F(ProfileSyncServiceTest, GetUserNoisedBirthYearAndGender_SyncEnabled) {
  // Initialize service with sync enabled.
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const int user_demographics_birth_year =
      kOldEnoughForDemographicsUserBirthYear;
  const int birth_year_offset = 2;
  const metrics::UserDemographicsProto_Gender user_demographics_gender =
      metrics::UserDemographicsProto_Gender_GENDER_FEMALE;

  // Set demographic prefs that are normally fetched from server when syncing.
  SetDemographics(user_demographics_birth_year, user_demographics_gender);

  // Directly set birth year offset in demographic prefs to avoid it being set
  // with a random value when calling GetUserNoisedBirthYearAndGender().
  prefs()->SetInteger(prefs::kSyncDemographicsBirthYearOffset,
                      birth_year_offset);

  UserDemographicsResult user_demographics_result =
      service()->GetUserNoisedBirthYearAndGender(GetNowTime());
  ASSERT_TRUE(user_demographics_result.IsSuccess());
  EXPECT_EQ(user_demographics_birth_year + birth_year_offset,
            user_demographics_result.value().birth_year);
  EXPECT_EQ(user_demographics_gender, user_demographics_result.value().gender);
}

// Test whether sync service does not provide user demographics when sync is
// turned off.
TEST_F(ProfileSyncServiceTest, GetUserNoisedBirthYearAndGender_SyncTurnedOff) {
  // Initialize service with sync disabled because no sign-in.
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Set demographic prefs that should normally be cleared when sync is
  // disabled. We keep the demographic prefs available in this test to make
  // sure that they are not provided when sync is disabled (we want
  // base::nullopt in any case).
  SetDemographics(kOldEnoughForDemographicsUserBirthYear,
                  metrics::UserDemographicsProto_Gender_GENDER_FEMALE);

  // Verify that demographic prefs exist (i.e., the test is set up).
  ASSERT_TRUE(HasBirthYearDemographic(prefs()));
  ASSERT_TRUE(HasGenderDemographic(prefs()));

  // Verify that we don't get demographics when sync is off.
  EXPECT_FALSE(
      service()->GetUserNoisedBirthYearAndGender(GetNowTime()).IsSuccess());
}

// Test whether sync service does not provide user demographics and does not
// clear demographic prefs when sync is temporarily disabled.
TEST_F(ProfileSyncServiceTest,
       GetUserNoisedBirthYearAndGender_SyncTemporarilyDisabled) {
  // Initialize service with sync enabled at start.
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const int user_demographics_birth_year =
      kOldEnoughForDemographicsUserBirthYear;
  const int birth_year_offset = 2;
  const metrics::UserDemographicsProto_Gender user_demographics_gender =
      metrics::UserDemographicsProto_Gender_GENDER_FEMALE;

  // Set demographic prefs that are normally fetched from server when syncing.
  SetDemographics(user_demographics_birth_year, user_demographics_gender);

  // Set birth year noise offset that is usually set when calling
  // SyncPrefs::GetUserNoisedBirthYearAndGender().
  prefs()->SetInteger(prefs::kSyncDemographicsBirthYearOffset,
                      static_cast<int>(birth_year_offset));

  // Verify that demographic prefs exist (i.e., the test is set up).
  ASSERT_TRUE(HasBirthYearDemographic(prefs()));
  ASSERT_TRUE(HasGenderDemographic(prefs()));
  ASSERT_TRUE(HasBirthYearOffset(prefs()));

  // Temporarily disable sync without turning it off.
  service()->GetUserSettings()->SetSyncRequested(false);
  ASSERT_FALSE(service()->GetUserSettings()->IsSyncRequested());
  ASSERT_EQ(SyncService::DISABLE_REASON_USER_CHOICE,
            service()->GetDisableReasons());

  // Verify that sync service does not provide demographics when it is
  // temporarily disabled.
  UserDemographicsResult user_demographics_result =
      service()->GetUserNoisedBirthYearAndGender(GetNowTime());
  EXPECT_FALSE(user_demographics_result.IsSuccess());

  // Verify that demographic prefs are not cleared.
  EXPECT_TRUE(HasBirthYearDemographic(prefs()));
  EXPECT_TRUE(HasGenderDemographic(prefs()));
  EXPECT_TRUE(HasBirthYearOffset(prefs()));
}

// Test whether sync service does not provide user demographics and does not
// clear demographic prefs when sync is paused and enabled, which represents the
// case where the kStopSyncInPausedState feature is disabled.
TEST_F(ProfileSyncServiceTest,
       GetUserNoisedBirthYearAndGender_SyncPausedAndFeatureDisabled) {
  base::test::ScopedFeatureList feature;
  // Disable the feature that stops the sync engine (disables sync) when sync is
  // paused.
  feature.InitAndDisableFeature(switches::kStopSyncInPausedState);

  // Initialize service with sync enabled at start.
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Set demographic prefs that are normally fetched from server when syncing.
  SetDemographics(kOldEnoughForDemographicsUserBirthYear,
                  metrics::UserDemographicsProto_Gender_GENDER_FEMALE);

  // Set birth year noise offset that is usually set when calling
  // SyncPrefs::GetUserNoisedBirthYearAndGender().
  prefs()->SetInteger(prefs::kSyncDemographicsBirthYearOffset, 2);

  // Verify that demographic prefs exist (i.e., the test is set up).
  ASSERT_TRUE(HasBirthYearDemographic(prefs()));
  ASSERT_TRUE(HasGenderDemographic(prefs()));
  ASSERT_TRUE(HasBirthYearOffset(prefs()));

  // Simulate sign out using an invalid auth error.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  ASSERT_EQ(SyncService::DISABLE_REASON_NONE, service()->GetDisableReasons());

  // Verify that sync service does not provide demographics when sync is paused.
  UserDemographicsResult user_demographics_result =
      service()->GetUserNoisedBirthYearAndGender(GetNowTime());
  EXPECT_FALSE(user_demographics_result.IsSuccess());

  // Verify that demographic prefs are not cleared.
  EXPECT_TRUE(HasBirthYearDemographic(prefs()));
  EXPECT_TRUE(HasGenderDemographic(prefs()));
  EXPECT_TRUE(HasBirthYearOffset(prefs()));
}

// Test whether sync service does not provide user demographics and does not
// clear demographic prefs when sync is paused and disabled, which represents
// the case where the kStopSyncInPausedState feature is enabled.
TEST_F(ProfileSyncServiceTest,
       GetUserNoisedBirthYearAndGender_SyncPausedAndFeatureEnabled) {
  base::test::ScopedFeatureList feature;
  // Enable the feature that stops the sync engine (disables sync) when sync is
  // paused.
  feature.InitAndEnableFeature(switches::kStopSyncInPausedState);

  // Initialize service with sync enabled at start.
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Set demographic prefs that are normally fetched from server when syncing.
  SetDemographics(kOldEnoughForDemographicsUserBirthYear,
                  metrics::UserDemographicsProto_Gender_GENDER_FEMALE);

  // Set birth year noise offset that is usually set when calling
  // SyncPrefs::GetUserNoisedBirthYearAndGender().
  prefs()->SetInteger(prefs::kSyncDemographicsBirthYearOffset, 2);

  // Verify that demographic prefs exist (i.e., the test is set up).
  ASSERT_TRUE(HasBirthYearDemographic(prefs()));
  ASSERT_TRUE(HasGenderDemographic(prefs()));
  ASSERT_TRUE(HasBirthYearOffset(prefs()));

  // Simulate sign out using an invalid auth error.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  ASSERT_EQ(SyncService::DISABLE_REASON_PAUSED, service()->GetDisableReasons());

  // Verify that sync service does not provide demographics when sync is paused.
  UserDemographicsResult user_demographics_result =
      service()->GetUserNoisedBirthYearAndGender(GetNowTime());
  EXPECT_FALSE(user_demographics_result.IsSuccess());

  // Verify that demographic prefs are not cleared.
  EXPECT_TRUE(HasBirthYearDemographic(prefs()));
  EXPECT_TRUE(HasGenderDemographic(prefs()));
  EXPECT_TRUE(HasBirthYearOffset(prefs()));
}

TEST_F(ProfileSyncServiceTest, GetExperimentalAuthenticationKey) {
  const std::vector<uint8_t> kExpectedPrivateKeyInfo = {
      0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
      0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
      0x03, 0x01, 0x07, 0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20,
      0xae, 0xf3, 0x15, 0x62, 0x31, 0x99, 0x3f, 0xe2, 0x96, 0xd4, 0xe6, 0x9c,
      0x33, 0x25, 0x38, 0x58, 0x97, 0xcc, 0x40, 0x0d, 0xab, 0xbf, 0x2b, 0xb7,
      0xd4, 0xcd, 0x79, 0xb9, 0x1f, 0x95, 0x19, 0x66, 0xa1, 0x44, 0x03, 0x42,
      0x00, 0x04, 0x5e, 0xf4, 0x5d, 0x00, 0xaa, 0xea, 0xc9, 0x33, 0xed, 0xcd,
      0xe5, 0xaf, 0xe6, 0x42, 0xef, 0x2b, 0xd2, 0xe0, 0xd6, 0x74, 0x5c, 0x90,
      0x45, 0xad, 0x3f, 0x60, 0xfd, 0xc1, 0xcd, 0x09, 0x0a, 0x9a, 0xda, 0x3d,
      0xf8, 0x18, 0xc6, 0x16, 0x46, 0x79, 0x53, 0x75, 0x92, 0xf2, 0x77, 0xcc,
      0x38, 0x65, 0xa1, 0xcc, 0x79, 0xb3, 0x06, 0xd9, 0x9c, 0xb6, 0x8b, 0x96,
      0x33, 0x88, 0x09, 0xc4, 0x07, 0x44};

  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  const std::string kSeparator("|");
  const std::string kGaiaId = signin::GetTestGaiaIdForEmail(kTestUser);
  const std::string expected_secret =
      kGaiaId + kSeparator + FakeSyncEngine::kTestBirthday + kSeparator +
      FakeSyncEngine::kTestKeystoreKey;

  EXPECT_EQ(expected_secret,
            service()->GetExperimentalAuthenticationSecretForTest());

  std::unique_ptr<crypto::ECPrivateKey> actual_key =
      service()->GetExperimentalAuthenticationKey();
  ASSERT_TRUE(actual_key);
  std::vector<uint8_t> actual_private_key;
  EXPECT_TRUE(actual_key->ExportPrivateKey(&actual_private_key));
  EXPECT_EQ(kExpectedPrivateKeyInfo, actual_private_key);
}

TEST_F(ProfileSyncServiceTest, GetExperimentalAuthenticationKeyLocalSync) {
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  EXPECT_TRUE(service()->GetExperimentalAuthenticationSecretForTest().empty());
  EXPECT_FALSE(service()->GetExperimentalAuthenticationKey());
}

}  // namespace
}  // namespace syncer
