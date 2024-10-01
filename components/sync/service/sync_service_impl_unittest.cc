// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_service_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "components/sync/test/fake_sync_engine.h"
#include "components/sync/test/fake_sync_engine_factory.h"
#include "components/sync/test/mock_data_type_local_data_batch_uploader.h"
#include "components/sync/test/sync_client_mock.h"
#include "components/sync/test/sync_service_impl_bundle.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::AtLeast;
using testing::ByMove;
using testing::ContainerEq;
using testing::Contains;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

namespace syncer {

namespace {

constexpr char kTestUser[] = "test_user@gmail.com";

// Construction parameters for FakeDataTypeController.
struct FakeControllerInitParams {
  DataType data_type;
  bool enable_transport_mode = false;
  std::unique_ptr<DataTypeLocalDataBatchUploader> batch_uploader;
};

MATCHER_P(ContainsDataType, type, "") {
  return arg.Has(type);
}

MATCHER_P(IsValidFieldTrialGroupWithName, expected_name, "") {
  return arg.is_valid() && arg.name() == expected_name;
}

SyncCycleSnapshot MakeDefaultSyncCycleSnapshot() {
  // It doesn't matter what exactly we set here, it's only relevant that the
  // SyncCycleSnapshot is initialized at all.
  return SyncCycleSnapshot(
      /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
      syncer::ModelNeutralState(), syncer::ProgressMarkerMap(),
      /*is_silenced=*/false,
      /*num_server_conflicts=*/0,
      /*notifications_enabled=*/true,
      /*sync_start_time=*/base::Time::Now(),
      /*poll_finish_time=*/base::Time::Now(),
      sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*poll_interval=*/base::Minutes(1),
      /*has_remaining_local_changes=*/false);
}

class MockSyncServiceObserver : public SyncServiceObserver {
 public:
  MOCK_METHOD(void, OnStateChanged, (SyncService * sync), (override));
};

class TestSyncServiceObserver : public SyncServiceObserver {
 public:
  TestSyncServiceObserver() = default;

  void OnStateChanged(SyncService* sync) override {
    setup_in_progress_ = sync->IsSetupInProgress();
    auth_error_ = sync->GetAuthError();
  }

  bool setup_in_progress() const { return setup_in_progress_; }
  GoogleServiceAuthError auth_error() const { return auth_error_; }

 private:
  bool setup_in_progress_ = false;
  GoogleServiceAuthError auth_error_;
};

// A test harness that uses a real SyncServiceImpl and in most cases a
// FakeSyncEngine.
//
// This is useful if we want to test the SyncServiceImpl and don't care about
// testing the SyncEngine.
class SyncServiceImplTest : public ::testing::Test {
 protected:
  SyncServiceImplTest() = default;
  ~SyncServiceImplTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kSyncDeferredStartupTimeoutSeconds, "0");
  }

  void TearDown() override {
    // Kill the service before the profile.
    ShutdownAndReleaseService();
  }

  signin::GaiaIdHash gaia_id_hash() {
    return signin::GaiaIdHash::FromGaiaId(
        identity_test_env()
            ->identity_manager()
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .gaia);
  }

  void SignInWithoutSyncConsent() {
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSignin);
  }

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  void SignInWithSyncConsent() {
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSync);
  }

  void InitializeService() {
    std::vector<FakeControllerInitParams> params;
    params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
    params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
    InitializeService(std::move(params));
  }

  void InitializeService(std::vector<FakeControllerInitParams>
                             registered_types_controller_params) {
    DCHECK(!service_);

    // Default includes a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    for (auto& params : registered_types_controller_params) {
      auto controller = std::make_unique<FakeDataTypeController>(
          params.data_type, params.enable_transport_mode,
          std::move(params.batch_uploader));
      // Hold a raw pointer to directly interact with the controller.
      controller_map_[params.data_type] = controller.get();
      controllers.push_back(std::move(controller));
    }

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, IsPasswordSyncAllowed).WillByDefault(Return(true));
    ON_CALL(*sync_client, GetIdentityManager)
        .WillByDefault(Return(identity_manager()));

    service_ = std::make_unique<SyncServiceImpl>(
        sync_service_impl_bundle_.CreateBasicInitParams(
            std::move(sync_client)));
    service_->Initialize(std::move(controllers));
  }

  void InitializeServiceWithLocalSyncBackend() {
    DCHECK(!service_);

    // Include a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    controllers.push_back(std::make_unique<FakeDataTypeController>(BOOKMARKS));
    controllers.push_back(std::make_unique<FakeDataTypeController>(
        DEVICE_INFO, /*enable_transport_only_modle=*/true));

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, GetIdentityManager)
        .WillByDefault(Return(identity_manager()));

    SyncServiceImpl::InitParams init_params =
        sync_service_impl_bundle_.CreateBasicInitParams(std::move(sync_client));

    prefs()->SetBoolean(prefs::kEnableLocalSyncBackend, true);

    service_ = std::make_unique<SyncServiceImpl>(std::move(init_params));
    service_->Initialize(std::move(controllers));
  }

  std::unique_ptr<SyncServiceImpl> ShutdownAndReleaseService() {
    if (service_) {
      service_->Shutdown();
    }
    return std::move(service_);
  }

  void PopulatePrefsForInitialSyncFeatureSetupComplete() {
    CHECK(!service_);
    engine_factory()->set_first_time_sync_configure_done(true);
    // Set first sync time before initialize to simulate a complete sync setup.
    SyncPrefs sync_prefs(prefs());
    sync_prefs.SetSelectedTypesForSyncingUser(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/UserSelectableTypeSet::All());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_TRUE(sync_prefs.IsInitialSyncFeatureSetupComplete());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
    sync_prefs.SetInitialSyncFeatureSetupComplete();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SetInvalidationsEnabled() {
    SyncStatus status = engine()->GetDetailedStatus();
    status.notifications_enabled = true;
    engine()->SetDetailedStatus(status);
    service()->OnInvalidationStatusChanged();
  }

  void TriggerPassphraseRequired() {
    service_->GetEncryptionObserverForTest()->OnPassphraseRequired(
        KeyDerivationParams::CreateForPbkdf2(), sync_pb::EncryptedData());
  }

  signin::IdentityManager* identity_manager() {
    return sync_service_impl_bundle_.identity_manager();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return sync_service_impl_bundle_.identity_test_env();
  }

  SyncServiceImpl* service() { return service_.get(); }

  SyncClientMock* sync_client() { return sync_client_; }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return sync_service_impl_bundle_.pref_service();
  }

  FakeSyncEngineFactory* engine_factory() {
    return sync_service_impl_bundle_.engine_factory();
  }

  FakeSyncEngine* engine() { return engine_factory()->last_created_engine(); }

  MockSyncInvalidationsService* sync_invalidations_service() {
    return sync_service_impl_bundle_.sync_invalidations_service();
  }

  trusted_vault::FakeTrustedVaultClient* trusted_vault_client() {
    return sync_service_impl_bundle_.trusted_vault_client();
  }

  FakeDataTypeController* get_controller(DataType type) {
    return controller_map_[type];
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncEnableModelTypeLocalDataBatchUploaders};
  SyncServiceImplBundle sync_service_impl_bundle_;
  std::unique_ptr<SyncServiceImpl> service_;
  raw_ptr<SyncClientMock, DanglingUntriaged> sync_client_ =
      nullptr;  // Owned by |service_|.
  // The controllers are owned by |service_|.
  std::map<DataType, raw_ptr<FakeDataTypeController, CtnExperimental>>
      controller_map_;
};

// Verify that the server URLs are sane.
TEST_F(SyncServiceImplTest, InitialState) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  const std::string& url = service()->GetSyncServiceUrlForDebugging().spec();
  EXPECT_TRUE(url == internal::kSyncServerUrl ||
              url == internal::kSyncDevServerUrl);
}

TEST_F(SyncServiceImplTest, SuccessfulInitialization) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest, SuccessfulLocalBackendInitialization) {
  InitializeServiceWithLocalSyncBackend();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// ChromeOS Ash sets FirstSetupComplete automatically.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Verify that an initialization where first setup is not complete does not
// start up Sync-the-feature.
TEST_F(SyncServiceImplTest, NeedsConfirmation) {
  // Mimic the sync setup being pending (SetInitialSyncFeatureSetupComplete()
  // not invoked).
  SyncPrefs sync_prefs(prefs());
  sync_prefs.SetSelectedTypesForSyncingUser(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());

  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().empty());

  // Sync should immediately start up in transport mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}
#endif

TEST_F(SyncServiceImplTest, DataTypesForTransportMode) {
  SignInWithoutSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Sync-the-feature is normally enabled in Ash. Triggering a dashboard reset
  // is one way to achieve otherwise.
  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableProtocolError(client_cmd);
#endif

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  // Sync-the-transport should become active.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // DataTypes for sync-the-feature are not configured.
  EXPECT_FALSE(service()->GetActiveDataTypes().Has(BOOKMARKS));

  // DataTypes for sync-the-transport are configured.
  EXPECT_TRUE(service()->GetActiveDataTypes().Has(DEVICE_INFO));
}

// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(SyncServiceImplTest, SetupInProgress) {
  InitializeService();
  base::RunLoop().RunUntilIdle();

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  std::unique_ptr<SyncSetupInProgressHandle> sync_blocker =
      service()->GetSetupInProgressHandle();
  EXPECT_TRUE(observer.setup_in_progress());
  sync_blocker.reset();
  EXPECT_FALSE(observer.setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that disable by enterprise policy works.
TEST_F(SyncServiceImplTest, DisabledByPolicyBeforeInit) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest, DisabledByPolicyBeforeInitThenPolicyRemoved) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();

  InitializeService();
  base::RunLoop().RunUntilIdle();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // Remove the policy.
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));
  base::RunLoop().RunUntilIdle();

  // The transport becomes active, but sync-the-feature remains off until the
  // user takes some action, where the precise action depends on the platform.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(service()->GetDisableReasons().empty());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS Ash, the first setup is marked as complete automatically.
  ASSERT_TRUE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());

  // On ChromeOS Ash, sync-the-feature stays disabled even after the policy is
  // removed, for historic reasons. It is unclear if this behavior is optional,
  // because it is indistinguishable from the sync-reset-via-dashboard case.
  // It can be resolved by invoking SetSyncFeatureRequested().
  EXPECT_TRUE(
      service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard());
  service()->SetSyncFeatureRequested();

#else
  // For any platform except ChromeOS Ash, the user needs to turn sync on
  // manually.
  ASSERT_FALSE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  service()->SetSyncFeatureRequested();
  service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  base::RunLoop().RunUntilIdle();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Sync-the-feature is considered on.
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(SyncServiceImplTest, DisabledByPolicyAfterInit) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

// Exercises the SyncServiceImpl's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(SyncServiceImplTest, AbortedByShutdown) {
  engine_factory()->AllowFakeEngineInitCompletion(false);
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  ShutdownAndReleaseService();
}

// Certain SyncServiceImpl tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out".
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Test the user signing out before the backend's initialization completes.
TEST_F(SyncServiceImplTest, EarlySignOut) {
  // Set up a fake sync engine that will not immediately finish initialization.
  engine_factory()->AllowFakeEngineInitCompletion(false);
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Certain SyncServiceImpl tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out".
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, SignOutDisablesSyncTransportAndSyncFeature) {
  // Sign-in and enable sync.
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest,
       SignOutClearsSyncTransportDataAndSyncTheFeaturePrefs) {
  // Sign-in and enable sync.
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  ASSERT_EQ(SyncService::DisableReasonSet(), service()->GetDisableReasons());
  ASSERT_TRUE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  // These are specific to sync-the-feature and should be cleared.
  EXPECT_FALSE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            service()->GetDisableReasons());
  EXPECT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));
}

TEST_F(SyncServiceImplTest,
       SignOutDuringTransportModeClearsTransportDataAndAccountStorageOptIn) {
  // Sign-in.
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  // Sync-the-transport should become active.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(
    SyncServiceImplTest,
    AddressesSyncShouldBeDisabledForNewlySigninUsersWithAlreadyCustomPassphraseSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {syncer::kReplaceSyncPromosWithSignInPromos,
       syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
       syncer::kSyncEnableContactInfoDataTypeInTransportMode},
      /*disabled_features=*/{});

  // Sign-in.
  SignInWithoutSyncConsent();
  // Registering CONTACT_INFO which includes addresses.
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(CONTACT_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Sync-the-feature is normally enabled in Ash. Triggering a dashboard reset
  // is one way to achieve otherwise.
  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableProtocolError(client_cmd);
#endif

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // This call represents the initial passphrase type coming in from the server.
  service()->PassphraseTypeChanged(PassphraseType::kCustomPassphrase);

  // UserSelectableType::kAutofill should have been disabled.
  EXPECT_FALSE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));

  // The user enables addresses sync.
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, true);

  // UserSelectableType::kAutofill should have been enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));

  // This call represents the passphrase type being determined again after a
  // browser restart.
  service()->PassphraseTypeChanged(PassphraseType::kCustomPassphrase);

  // UserSelectableType::kAutofill should stay enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(
    SyncServiceImplTest,
    AddressesSyncValueShouldRemainUnchangedForCustomPassphraseUsersAfterTheirInitialSignin) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {syncer::kReplaceSyncPromosWithSignInPromos,
       syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
       syncer::kSyncEnableContactInfoDataTypeInTransportMode},
      /*disabled_features=*/{});

  // Sign-in.
  SignInWithoutSyncConsent();
  // Registering CONTACT_INFO which includes addresses.
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(CONTACT_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // This call represents the initial passphrase type coming in from the server.
  service()->PassphraseTypeChanged(PassphraseType::kCustomPassphrase);

  // UserSelectableType::kAutofill should have been disabled.
  EXPECT_FALSE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));

  // The user enables addresses sync.
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, true);

  // UserSelectableType::kAutofill should have been enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();

  // Sign-in.
  SignInWithoutSyncConsent();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // This call represents the initial passphrase type coming in from the server.
  service()->PassphraseTypeChanged(PassphraseType::kCustomPassphrase);

  // UserSelectableType::kAutofill should stay enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(
    SyncServiceImplTest,
    AddressesSyncShouldNotBeDisabledForSignedInUsersWithNewlyCustomPassphraseSet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/
      {syncer::kReplaceSyncPromosWithSignInPromos,
       syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
       switches::kExplicitBrowserSigninUIOnDesktop,
#endif
       syncer::kSyncEnableContactInfoDataTypeInTransportMode},
      /*disabled_features=*/{});

  // Sign-in.
  SignInWithoutSyncConsent();
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  ASSERT_TRUE(prefs()->GetBoolean(::prefs::kExplicitBrowserSignin));
#endif

  // Registering CONTACT_INFO which includes addresses.
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(CONTACT_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Sync-the-feature is normally enabled in Ash. Triggering a dashboard reset
  // is one way to achieve otherwise.
  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableProtocolError(client_cmd);
#endif

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // This call represents the initial passphrase type coming in from the server,
  // the user has no custom passphrase before signing in.
  service()->PassphraseTypeChanged(PassphraseType::kKeystorePassphrase);

  // UserSelectableType::kAutofill should have been enabled.
  ASSERT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));

  // This call represents setting a custom passphrase either locally or coming
  // in from the server.
  service()->PassphraseTypeChanged(PassphraseType::kCustomPassphrase);

  // UserSelectableType::kAutofill should stay enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));
}

TEST_F(SyncServiceImplTest, GetSyncTokenStatus) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  // Initial status: The Sync engine startup has not begun yet; no token request
  // has been sent.
  SyncTokenStatus token_status = service()->GetSyncTokenStatusForDebugging();
  ASSERT_EQ(CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  ASSERT_TRUE(token_status.connection_status_update_time.is_null());
  ASSERT_TRUE(token_status.token_request_time.is_null());
  ASSERT_TRUE(token_status.token_response_time.is_null());
  ASSERT_FALSE(token_status.has_token);

  // Sync engine startup as well as the actual token request take the form of
  // posted tasks. Run them.
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

TEST_F(SyncServiceImplTest, RevokeAccessTokenFromTokenService) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // TODO(crbug.com/40066949): Update once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

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
TEST_F(SyncServiceImplTest, CredentialsRejectedByClient_StopSync) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

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
  EXPECT_FALSE(service()->IsEngineInitialized());
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// CrOS Ash does not support signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, SignOutRevokeAccessToken) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();

  // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
  DCHECK(account_mutator);

  account_mutator->ClearPrimaryAccount(signin_metrics::ProfileSignout::kTest);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

// Verify that sync transport data is cleared when the service is initializing
// and account is signed out.
// This code path doesn't exist on ChromeOS-Ash, since signout is not possible.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, ClearTransportDataOnInitializeWhenSignedOut) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();

  // Clearing prefs can be triggered only after `IdentityManager` finishes
  // loading the list of accounts, so wait for it to complete.
  identity_test_env()->WaitForRefreshTokensLoaded();

  ASSERT_TRUE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));

  // Don't sign-in before creating the service.
  InitializeService();

  EXPECT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplTest, DashboardResetTwiceDoesNotCrash) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Disable sync via dashboard (https://chrome.google.com/sync).
  service()->OnActionableProtocolError(
      {.error_type = NOT_MY_BIRTHDAY, .action = DISABLE_SYNC_ON_CLIENT});

  // Sync-the-feature is off.
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  // Resetting a second time should not crash.
  service()->OnActionableProtocolError(
      {.error_type = NOT_MY_BIRTHDAY, .action = DISABLE_SYNC_ON_CLIENT});
}

// Verify that credential errors get returned from GetAuthError().
TEST_F(SyncServiceImplTest, CredentialErrorReturned) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for SyncServiceImpl to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            observer.auth_error().state());
  // Sync should pause.
  EXPECT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

TEST_F(SyncServiceImplTest,
       TransportIsDisabledIfBothAuthErrorAndDisableReason) {
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(service()->GetDisableReasons(),
            SyncService::DisableReasonSet{
                SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // The lower transport state (DISABLED) should be returned.
  EXPECT_EQ(service()->GetTransportState(),
            SyncService::TransportState::DISABLED);

  // Remove the disable reason.
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));
  ASSERT_TRUE(service()->GetDisableReasons().empty());

  // Transport is now PAUSED.
  EXPECT_EQ(service()->GetTransportState(),
            SyncService::TransportState::PAUSED);
}

// Verify that credential errors get cleared when a new token is fetched
// successfully.
TEST_F(SyncServiceImplTest, CredentialErrorClearsOnNewToken) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  identity_test_env()->SetAutomaticIssueOfAccessTokens(false);

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);

  // Make sure the expected account_id was passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, engine()->authenticated_account_id());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  // Wait for SyncServiceImpl to send an access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      primary_account_id, "access token", base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Wait for SyncServiceImpl to be notified of the changed credentials and
  // send a new access token request.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  // Sync should pause.
  ASSERT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  // Now emulate Chrome receiving a new, valid LST.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();
  // Again, wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "this one works", base::Time::Now() + base::Days(10));

  // Check that sync auth error state cleared.
  EXPECT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::NONE, observer.auth_error().state());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  service()->RemoveObserver(&observer);
}

// Verify that the disable sync flag disables sync.
TEST_F(SyncServiceImplTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(kDisableSync);
  EXPECT_FALSE(IsSyncAllowedByFlag());
}

// Verify that no disable sync flag enables sync.
TEST_F(SyncServiceImplTest, NoDisableSyncFlag) {
  EXPECT_TRUE(IsSyncAllowedByFlag());
}

// Test that when SyncServiceImpl receives actionable error
// RESET_LOCAL_SYNC_DATA it restarts sync.
TEST_F(SyncServiceImplTest, ResetLocalSyncData) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());
  ASSERT_EQ(service()->GetActiveDataTypes(),
            DataTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}));

  SyncProtocolError client_cmd;
  client_cmd.action = RESET_LOCAL_SYNC_DATA;
  service()->OnActionableProtocolError(client_cmd);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(service()->GetActiveDataTypes(),
            DataTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}));
  EXPECT_GT(get_controller(BOOKMARKS)->model()->clear_metadata_count(), 0);
}

// Test that when SyncServiceImpl receives actionable error
// DISABLE_SYNC_ON_CLIENT it disables sync and restarts the engine in transport
// mode.
TEST_F(SyncServiceImplTest, DisableSyncOnClient) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(
      service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // TODO(crbug.com/40066949): Update once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  const std::string primary_account_gaia_id =
      identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .gaia;
  // Store some trusted vault keys explicitly to verify that trusted vault local
  // state is cleared upon DISABLE_SYNC_ON_CLIENT.
  trusted_vault_client()->StoreKeys(
      primary_account_gaia_id, /*keys=*/{{1, 2, 3}}, /*last_key_version=*/1);
  ASSERT_THAT(trusted_vault_client()->GetStoredKeys(primary_account_gaia_id),
              Not(IsEmpty()));

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableProtocolError(client_cmd);

  EXPECT_FALSE(
      engine_factory()->HasTransportDataIncludingFirstSync(gaia_id_hash()));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ash does not support signout.
  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(service()->GetDisableReasons().empty());
  // Since ChromeOS doesn't support signout and so the account is still there
  // and available, Sync will restart in standalone transport mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_TRUE(
      service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard());
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On iOS and Android, the primary account is cleared.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());
  EXPECT_TRUE(service()->GetLastSyncedTimeForDebugging().is_null());
#else
  // On Desktop and Lacros, the sync consent is revoked, but the primary account
  // is left at ConsentLevel::kSignin. Sync will restart in standalone transport
  // mode.
  // TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(service()->GetDisableReasons().empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // TrustedVault data should have been cleared.
  EXPECT_THAT(trusted_vault_client()->GetStoredKeys(primary_account_gaia_id),
              IsEmpty());

  EXPECT_GT(get_controller(BOOKMARKS)->model()->clear_metadata_count(), 0);

  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
}

TEST_F(SyncServiceImplTest,
       DisableSyncOnClientLogsPassphraseTypeForNotMyBirthday) {
  const PassphraseType kPassphraseType = PassphraseType::kKeystorePassphrase;

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  service()->GetEncryptionObserverForTest()->OnPassphraseTypeChanged(
      kPassphraseType, /*passphrase_time=*/base::Time());

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(kPassphraseType, service()->GetUserSettings()->GetPassphraseType());

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  client_cmd.error_type = NOT_MY_BIRTHDAY;

  base::HistogramTester histogram_tester;
  service()->OnActionableProtocolError(client_cmd);

  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseTypeUponNotMyBirthdayOrEncryptionObsolete",
      /*sample=*/kPassphraseType,
      /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceImplTest,
       DisableSyncOnClientLogsPassphraseTypeForEncryptionObsolete) {
  const PassphraseType kPassphraseType = PassphraseType::kKeystorePassphrase;

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  service()->GetEncryptionObserverForTest()->OnPassphraseTypeChanged(
      kPassphraseType, /*passphrase_time=*/base::Time());

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(service()->IsSyncFeatureEnabled());
  ASSERT_EQ(kPassphraseType, service()->GetUserSettings()->GetPassphraseType());

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  client_cmd.error_type = ENCRYPTION_OBSOLETE;

  base::HistogramTester histogram_tester;
  service()->OnActionableProtocolError(client_cmd);

  ASSERT_FALSE(service()->IsSyncFeatureEnabled());

  histogram_tester.ExpectUniqueSample(
      "Sync.PassphraseTypeUponNotMyBirthdayOrEncryptionObsolete",
      /*sample=*/kPassphraseType,
      /*expected_bucket_count=*/1);
}

TEST_F(SyncServiceImplTest, DisableSyncOnClientClearsPassphrasePrefForAccount) {
  const PassphraseType kPassphraseType = PassphraseType::kCustomPassphrase;

  SignInWithoutSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(AUTOFILL, /*enable_transport_mode=*/false);
  params.emplace_back(AUTOFILL_WALLET_DATA, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // This call represents the initial passphrase type coming in from the server.
  service()->PassphraseTypeChanged(kPassphraseType);
  ASSERT_EQ(kPassphraseType, service()->GetUserSettings()->GetPassphraseType());

  // Set the passphrase.
  SyncPrefs sync_prefs(prefs());
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
      identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia);
  sync_prefs.SetEncryptionBootstrapTokenForAccount("token", gaia_id_hash);
  ASSERT_EQ("token",
            sync_prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash));

  // Clear sync from the dashboard.
  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  client_cmd.error_type = NOT_MY_BIRTHDAY;
  service()->OnActionableProtocolError(client_cmd);

  // The passphrase for account pref cleared when sync is cleared from
  // dashboard.
  EXPECT_TRUE(
      sync_prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash).empty());
}

TEST_F(SyncServiceImplTest,
       DisableSyncOnClientClearsPassphrasePrefForSyncingAccount) {
  const PassphraseType kPassphraseType = PassphraseType::kCustomPassphrase;

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(AUTOFILL, /*enable_transport_mode=*/false);
  params.emplace_back(AUTOFILL_WALLET_DATA, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // This call represents the initial passphrase type coming in from the server.
  service()->PassphraseTypeChanged(kPassphraseType);
  ASSERT_EQ(kPassphraseType, service()->GetUserSettings()->GetPassphraseType());

  // Set the passphrase.
  SyncPrefs sync_prefs(prefs());
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
      identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia);
  sync_prefs.SetEncryptionBootstrapTokenForAccount("token", gaia_id_hash);
  ASSERT_EQ("token",
            sync_prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash));

  // Clear sync from the dashboard.
  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  client_cmd.error_type = NOT_MY_BIRTHDAY;
  service()->OnActionableProtocolError(client_cmd);

  // The passphrase for account pref cleared when sync is cleared from
  // dashboard.
  EXPECT_TRUE(
      sync_prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash).empty());
}

TEST_F(SyncServiceImplTest, EncryptionObsoleteClearsPassphrasePrefForAccount) {
  const PassphraseType kPassphraseType = PassphraseType::kCustomPassphrase;

  SignInWithoutSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(AUTOFILL, /*enable_transport_mode=*/false);
  params.emplace_back(AUTOFILL_WALLET_DATA, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // This call represents the initial passphrase type coming in from the server.
  service()->PassphraseTypeChanged(kPassphraseType);
  ASSERT_EQ(kPassphraseType, service()->GetUserSettings()->GetPassphraseType());

  // Set the passphrase.
  SyncPrefs sync_prefs(prefs());
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
      identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia);
  sync_prefs.SetEncryptionBootstrapTokenForAccount("token", gaia_id_hash);
  ASSERT_EQ("token",
            sync_prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash));

  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  client_cmd.error_type = ENCRYPTION_OBSOLETE;
  service()->OnActionableProtocolError(client_cmd);

  // The passphrase for account pref should be cleared.
  EXPECT_TRUE(
      sync_prefs.GetEncryptionBootstrapTokenForAccount(gaia_id_hash).empty());
}

// Verify a that local sync mode isn't impacted by sync being disabled.
TEST_F(SyncServiceImplTest, LocalBackendUnimpactedByPolicy) {
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));
  InitializeServiceWithLocalSyncBackend();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // The transport should continue active even if kSyncManaged becomes true.
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));

  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Setting kSyncManaged back to false should also make no difference.
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));

  EXPECT_TRUE(service()->GetDisableReasons().empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

// Test ConfigureDataTypeManagerReason on First and Nth start.
TEST_F(SyncServiceImplTest, ConfigureDataTypeManagerReason) {
  SignInWithSyncConsent();

  // First sync.
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT, engine()->last_configure_reason());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            engine()->last_configure_reason());
  ShutdownAndReleaseService();

  // Nth sync.
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            engine()->last_configure_reason());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            engine()->last_configure_reason());
  ShutdownAndReleaseService();
}

// Regression test for crbug.com/1043642, can be removed once
// SyncServiceImpl usages after shutdown are addressed.
TEST_F(SyncServiceImplTest, ShouldProvideDisableReasonsAfterShutdown) {
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<SyncServiceImpl> service = ShutdownAndReleaseService();
  EXPECT_FALSE(service->GetDisableReasons().empty());
}

TEST_F(SyncServiceImplTest, ShouldSendDataTypesToSyncInvalidationsService) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  // Note: Even though NIGORI technically isn't registered, it should always be
  // considered part of the interested data types.
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(NIGORI),
                                           ContainsDataType(BOOKMARKS),
                                           ContainsDataType(DEVICE_INFO))));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(engine()->started_handling_invalidations());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest,
       ShouldSendDataTypesToSyncInvalidationsServiceInTransportMode) {
  SignInWithoutSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));

  // In this test, BOOKMARKS doesn't support transport mode, so it should *not*
  // be included.
  // Note: Even though NIGORI technically isn't registered, it should always be
  // considered part of the interested data types.
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(NIGORI),
                                           Not(ContainsDataType(BOOKMARKS)),
                                           ContainsDataType(DEVICE_INFO))));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(engine()->started_handling_invalidations());
}
#else
TEST_F(SyncServiceImplTest,
       ShouldSendDataTypesToSyncInvalidationsServiceInTransportModeAsh) {
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // In this test, BOOKMARKS doesn't support transport mode, so it should *not*
  // be included.
  // Note: Even though NIGORI technically isn't registered, it should always be
  // considered part of the interested data types.
  // Note2: InitializeForFirstSync() issued a first SetInterestedDataTypes()
  // with sync-the-feature enabled, which we don't care about. That's why this
  // expectation is set afterwards.
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(NIGORI),
                                           Not(ContainsDataType(BOOKMARKS)),
                                           ContainsDataType(DEVICE_INFO))));

  // Sync-the-feature is normally enabled in Ash. Triggering a dashboard reset
  // is one way to achieve otherwise.
  SyncProtocolError client_cmd;
  client_cmd.action = DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableProtocolError(client_cmd);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_TRUE(engine()->started_handling_invalidations());
}
#endif

TEST_F(SyncServiceImplTest, ShouldEnableAndDisableInvalidationsForSessions) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(SESSIONS, /*enable_transport_mode=*/false);
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(ContainsDataType(SESSIONS)));
  service()->SetInvalidationsForSessionsEnabled(true);
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(Not(ContainsDataType(SESSIONS))));
  service()->SetInvalidationsForSessionsEnabled(false);
}

TEST_F(SyncServiceImplTest, ShouldNotSubscribeToProxyTypes) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  get_controller(BOOKMARKS)
      ->model()
      ->EnableSkipEngineConnectionForActivationResponse();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           Not(ContainsDataType(BOOKMARKS)))));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest, ShouldNotSubscribeToFailedTypes) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  get_controller(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Model error"));

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           Not(ContainsDataType(BOOKMARKS)))));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest, ShouldNotSubscribeToStopAndClearDataTypes) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  get_controller(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           Not(ContainsDataType(BOOKMARKS)))));
  base::RunLoop().RunUntilIdle();

  // Verify that data type is subscribed again when preconditions are met.
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           ContainsDataType(BOOKMARKS))));
  get_controller(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kPreconditionsMet);
  service()->DataTypePreconditionChanged(BOOKMARKS);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest, ShouldSubscribeToStopAndKeepDataTypes) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  get_controller(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndKeepData);

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           ContainsDataType(BOOKMARKS))));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest, ShouldUnsubscribeWhenStopAndClear) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           ContainsDataType(BOOKMARKS))));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           Not(ContainsDataType(BOOKMARKS)))));
  get_controller(BOOKMARKS)->SetPreconditionState(
      DataTypeController::PreconditionState::kMustStopAndClearData);
  service()->DataTypePreconditionChanged(BOOKMARKS);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest, ShouldUnsubscribeOnTypeFailure) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/false);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           ContainsDataType(BOOKMARKS))));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           Not(ContainsDataType(BOOKMARKS)))));
  get_controller(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Model error"));
  service()->DataTypePreconditionChanged(BOOKMARKS);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest,
       ShouldActivateSyncInvalidationsServiceWhenSyncIsInitialized) {
  SignInWithSyncConsent();
  InitializeService();

  // Invalidations may start listening twice. The first one during
  // initialization, the second once everything is configured.
  EXPECT_CALL(*sync_invalidations_service(), StartListening())
      .Times(AtLeast(1));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest,
       ShouldNotStartListeningInvalidationsWhenLocalSyncEnabled) {
  InitializeServiceWithLocalSyncBackend();
  EXPECT_CALL(*sync_invalidations_service(), StartListening()).Times(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncServiceImplTest,
       ShouldNotStopListeningPermanentlyOnShutdownBrowserAndKeepData) {
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*sync_invalidations_service(), StopListeningPermanently())
      .Times(0);
  ShutdownAndReleaseService();
}

TEST_F(SyncServiceImplTest, ShouldStopListeningPermanentlyOnDashboardReset) {
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*sync_invalidations_service(), StopListeningPermanently());
  service()->OnActionableProtocolError(
      {.error_type = NOT_MY_BIRTHDAY, .action = DISABLE_SYNC_ON_CLIENT});
}

TEST_F(SyncServiceImplTest, ShouldCallStopUponResetEngineIfAlreadyShutDown) {
  // The intention here is to stop sync without clearing metadata by getting to
  // a sync paused state by simulating a credential rejection error.

  // Sign in and enable sync.
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();

  // Simulate the credentials getting locally rejected by the client by setting
  // the refresh token to a special invalid value.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // The Sync engine should have been shut down.
  ASSERT_FALSE(service()->IsEngineInitialized());
  ASSERT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  EXPECT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_count());
  // Mimic resetting sync via the dashboard.
  // Clearing metadata should work even if the engine is not running.
  // On mobile the account is also removed from the device, so there are 2
  // clear counts.
  service()->OnActionableProtocolError(
      {.error_type = NOT_MY_BIRTHDAY, .action = DISABLE_SYNC_ON_CLIENT});
  EXPECT_GE(get_controller(BOOKMARKS)->model()->clear_metadata_count(), 1);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorDownloadStatus) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  get_controller(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Model error"));
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kError);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorDownloadStatusWhenSyncDisabled) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  // OnInvalidationStatusChanged() is used to only notify observers. This will
  // cause the histogram recorder to check data types status.
  service()->OnInvalidationStatusChanged();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kError);
}

TEST_F(SyncServiceImplTest, ShouldReturnWaitingDownloadStatus) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  bool met_configuring_data_type_manager = false;
  testing::NiceMock<MockSyncServiceObserver> mock_sync_service_observer;
  ON_CALL(mock_sync_service_observer, OnStateChanged)
      .WillByDefault(Invoke([&met_configuring_data_type_manager](
                                SyncService* service) {
        EXPECT_NE(service->GetDownloadStatusFor(syncer::BOOKMARKS),
                  SyncService::DataTypeDownloadStatus::kError);
        if (service->GetTransportState() ==
            SyncService::TransportState::CONFIGURING) {
          met_configuring_data_type_manager = true;
          EXPECT_EQ(service->GetDownloadStatusFor(syncer::BOOKMARKS),
                    SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
        }
      }));

  // Observers must be added after initialization has been started.
  ASSERT_THAT(engine(), IsNull());

  // GetDownloadStatusFor() must be called only after Initialize(), see
  // SyncServiceImpl::Initialize().
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  service()->AddObserver(&mock_sync_service_observer);
  base::RunLoop().RunUntilIdle();
  SetInvalidationsEnabled();

  EXPECT_TRUE(met_configuring_data_type_manager);
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kUpToDate);
  service()->RemoveObserver(&mock_sync_service_observer);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorWhenDataTypeDisabled) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  UserSelectableTypeSet enabled_types =
      service()->GetUserSettings()->GetSelectedTypes();
  enabled_types.Remove(UserSelectableType::kBookmarks);
  service()->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/false,
                                                 enabled_types);

  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kError);

  // Finish initialization and double check that the status hasn't changed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kError);

  SetInvalidationsEnabled();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kError);
}

TEST_F(SyncServiceImplTest, ShouldWaitUntilNoInvalidations) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  SetInvalidationsEnabled();

  SyncStatus status = engine()->GetDetailedStatus();
  status.invalidated_data_types.Put(BOOKMARKS);
  engine()->SetDetailedStatus(status);

  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::DEVICE_INFO),
            SyncService::DataTypeDownloadStatus::kUpToDate);
}

TEST_F(SyncServiceImplTest, ShouldWaitForInitializedInvalidations) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  SetInvalidationsEnabled();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kUpToDate);
}

TEST_F(SyncServiceImplTest, ShouldWaitForPollRequest) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  SetInvalidationsEnabled();

  ASSERT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kUpToDate);

  // OnInvalidationStatusChanged() is used to only notify observers, this is
  // required for metrics since they are calculated only when SyncService state
  // changes.
  engine()->SetPollIntervalElapsed(true);
  service()->OnInvalidationStatusChanged();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  engine()->SetPollIntervalElapsed(false);
  service()->OnInvalidationStatusChanged();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kUpToDate);

  // Ignore following poll requests once the first sync cycle is completed.
  service()->OnSyncCycleCompleted(MakeDefaultSyncCycleSnapshot());
  engine()->SetPollIntervalElapsed(true);
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kUpToDate);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorOnSyncPaused) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  ASSERT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

  // Mimic entering Sync paused state.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  // Expect the error status when Sync is paused.
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::DataTypeDownloadStatus::kError);
}

// These tests cover signing in after browser startup, which isn't supported on
// ChromeOS-Ash (where there's always a signed-in user).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(
    SyncServiceImplTest,
    GetTypesWithPendingDownloadForInitialSyncDuringFirstSyncInTransportMode) {
  engine_factory()->AllowFakeEngineInitCompletion(false);
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(AUTOFILL_WALLET_DATA, /*enable_transport_mode=*/true);
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  SignInWithoutSyncConsent();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // During first-sync INITIALIZING, all preferred datatypes are listed, which
  // in this test fixture means NIGORI, AUTOFILL_WALLET_DATA and DEVICE_INFO.
  EXPECT_EQ(DataTypeSet({NIGORI, AUTOFILL_WALLET_DATA, DEVICE_INFO}),
            service()->GetTypesWithPendingDownloadForInitialSync());

  // Once fully initialized, it is delegated to DataTypeManager.
  base::RunLoop().RunUntilIdle();
  engine()->TriggerInitializationCompletion(/*success=*/true);

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(DataTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());
}

TEST_F(SyncServiceImplTest,
       GetTypesWithPendingDownloadForInitialSyncDuringFirstSync) {
  engine_factory()->AllowFakeEngineInitCompletion(false);
  InitializeService();
  base::RunLoop().RunUntilIdle();
  SignInWithSyncConsent();

  service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // During first-sync INITIALIZING, all preferred datatypes are listed, which
  // in this test fixture means NIGORI, BOOKMARKS and DEVICE_INFO.
  EXPECT_EQ(DataTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}),
            service()->GetTypesWithPendingDownloadForInitialSync());

  // Once fully initialized, it is delegated to DataTypeManager.
  base::RunLoop().RunUntilIdle();
  engine()->TriggerInitializationCompletion(/*success=*/true);

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(DataTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplTest,
       GetTypesWithPendingDownloadForInitialSyncDuringNthSync) {
  engine_factory()->AllowFakeEngineInitCompletion(false);

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  ASSERT_EQ(SyncService::TransportState::START_DEFERRED,
            service()->GetTransportState());

  // During non-first-sync initialization, usually during profile startup,
  // SyncService doesn't actually know which datatypes are pending download, so
  // it defaults to returning an empty set.
  EXPECT_EQ(DataTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // Same as above.
  EXPECT_EQ(DataTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());

  // Once fully initialized, it is delegated to DataTypeManager.
  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(DataTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());
}

TEST_F(SyncServiceImplTest, EarlyCallToGetTypesWithUnsyncedDataShouldNotCrash) {
  InitializeService();
  base::MockCallback<base::OnceCallback<void(DataTypeSet)>> cb;
  EXPECT_CALL(cb, Run(DataTypeSet()));
  service()->GetTypesWithUnsyncedData(syncer::UserTypes(), cb.Get());
}

TEST_F(SyncServiceImplTest,
       ShouldNotForwardUponGetLocalDataDescriptionsIfSyncDisabled) {
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithoutSyncConsent();

  // DEVICE_INFO will be passed to GetLocalDataDescription(), but sync is
  // disabled by policy. So the uploader should not be queried.
  auto device_info_uploader =
      std::make_unique<MockDataTypeLocalDataBatchUploader>();
  EXPECT_CALL(*device_info_uploader, GetLocalDataDescription).Times(0);

  std::vector<FakeControllerInitParams> params;
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true,
                      std::move(device_info_uploader));
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  base::test::TestFuture<std::map<DataType, LocalDataDescription>> descriptions;
  service()->GetLocalDataDescriptions({DEVICE_INFO},
                                      descriptions.GetCallback());
  EXPECT_TRUE(descriptions.Wait());
}

TEST_F(SyncServiceImplTest,
       ShouldReturnEmptyUponGetLocalDataDescriptionsForSyncingUsers) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();

  // DEVICE_INFO will be passed to GetLocalDataDescription(), but the user is
  // syncing. So the uploader should not be queried.
  auto device_info_uploader =
      std::make_unique<MockDataTypeLocalDataBatchUploader>();
  EXPECT_CALL(*device_info_uploader, GetLocalDataDescription).Times(0);

  std::vector<FakeControllerInitParams> params;
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true,
                      std::move(device_info_uploader));
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service()->GetPreferredDataTypes().Has(DEVICE_INFO));

  base::test::TestFuture<std::map<DataType, LocalDataDescription>> descriptions;
  service()->GetLocalDataDescriptions({DEVICE_INFO},
                                      descriptions.GetCallback());

  EXPECT_THAT(descriptions.Get(), IsEmpty());
}

TEST_F(SyncServiceImplTest,
       ShouldNotForwardUponTriggerLocalDataMigrationIfSyncDisabled) {
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithoutSyncConsent();

  // DEVICE_INFO will be passed to TriggerLocalDataMigration(), but sync is
  // disabled by policy. So data should not be uploaded.
  auto device_info_uploader =
      std::make_unique<MockDataTypeLocalDataBatchUploader>();
  EXPECT_CALL(*device_info_uploader, TriggerLocalDataMigration).Times(0);

  std::vector<FakeControllerInitParams> params;
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true,
                      std::move(device_info_uploader));
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  service()->TriggerLocalDataMigration({DEVICE_INFO});
}

TEST_F(SyncServiceImplTest,
       ShouldDoNothingUponTriggerLocalDataMigrationForSyncingUsers) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();

  // DEVICE_INFO will be passed to TriggerLocalDataMigration(), but the user is
  // syncing. So data should not be uploaded.
  auto device_info_uploader =
      std::make_unique<MockDataTypeLocalDataBatchUploader>();
  EXPECT_CALL(*device_info_uploader, TriggerLocalDataMigration).Times(0);

  std::vector<FakeControllerInitParams> params;
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true,
                      std::move(device_info_uploader));
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(service()->GetPreferredDataTypes().Has(DEVICE_INFO));

  service()->TriggerLocalDataMigration({DEVICE_INFO});
}

TEST_F(SyncServiceImplTest, ShouldRecordLocalDataMigrationRequests) {
  base::HistogramTester histogram_tester;
  SignInWithoutSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(DEVICE_INFO, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  service()->TriggerLocalDataMigration({DEVICE_INFO, AUTOFILL_WALLET_DATA});

  // The metric records what was requested, regardless of what types are active.
  EXPECT_THAT(histogram_tester.GetAllSamples("Sync.BatchUpload.Requests3"),
              base::BucketsAre(
                  base::Bucket(DataTypeForHistograms::kDeviceInfo, 1),
                  base::Bucket(DataTypeForHistograms::kAutofillWalletData, 1)));
}

TEST_F(SyncServiceImplTest, ShouldNotifyOnManagedPrefDisabled) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();
  std::vector<FakeControllerInitParams> params;
  params.emplace_back(PASSWORDS, /*enable_transport_mode=*/true);
  params.emplace_back(BOOKMARKS, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  testing::NiceMock<MockSyncServiceObserver> mock_sync_service_observer;
  service()->AddObserver(&mock_sync_service_observer);

  EXPECT_CALL(mock_sync_service_observer, OnStateChanged);
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));

  service()->RemoveObserver(&mock_sync_service_observer);
}

TEST_F(SyncServiceImplTest, ShouldCacheTrustedVaultAutoUpgradeDebugInfo) {
  const int kTestCohort1 = 11;
  const int kTestCohort2 = 22;

  engine_factory()->AllowFakeEngineInitCompletion(false);
  InitializeService();
  base::RunLoop().RunUntilIdle();
  SignInWithSyncConsent();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_TRUE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());
  ASSERT_THAT(engine(), NotNull());

  {
    SyncStatus sync_status;
    sync_status.trusted_vault_debug_info
        .mutable_auto_upgrade_experiment_group()
        ->set_cohort(kTestCohort1);
    sync_status.trusted_vault_debug_info
        .mutable_auto_upgrade_experiment_group()
        ->set_type(sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL);
    engine()->SetDetailedStatus(sync_status);
  }

  // Completing initialization should exercise SyncClient's field trial
  // registration.
  EXPECT_CALL(*sync_client(),
              RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
                  IsValidFieldTrialGroupWithName("Cohort11_Control")));

  base::RunLoop().RunUntilIdle();
  engine()->TriggerInitializationCompletion(/*success=*/true);

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  testing::Mock::VerifyAndClearExpectations(sync_client());

  // Verify that the debug info has been cached in prefs.
  SyncPrefs sync_prefs(prefs());
  EXPECT_TRUE(
      sync_prefs.GetCachedTrustedVaultAutoUpgradeExperimentGroup().has_value());
  EXPECT_EQ(kTestCohort1,
            sync_prefs.GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                .cohort());
  EXPECT_EQ(sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
            sync_prefs.GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                .type());
  EXPECT_EQ(0, sync_prefs.GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                   .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                   .type_index());

  // The SyncClient API should not be invoked for the second time.
  EXPECT_CALL(*sync_client(),
              RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial)
      .Times(0);

  // Mimic another sync cycle that mutates the experiment group.
  {
    SyncStatus sync_status;
    sync_status.trusted_vault_debug_info
        .mutable_auto_upgrade_experiment_group()
        ->set_cohort(kTestCohort2);
    sync_status.trusted_vault_debug_info
        .mutable_auto_upgrade_experiment_group()
        ->set_type(sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL);
    engine()->SetDetailedStatus(sync_status);
    service()->OnSyncCycleCompleted(MakeDefaultSyncCycleSnapshot());
  }

  EXPECT_EQ(kTestCohort2,
            sync_prefs.GetCachedTrustedVaultAutoUpgradeExperimentGroup()
                .value_or(sync_pb::TrustedVaultAutoUpgradeExperimentGroup())
                .cohort());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, ShouldRecordHistoryOptInStateOnSignin) {
  // Allow UserSelectableType::kHistory in transport mode.
  base::test::ScopedFeatureList features{kReplaceSyncPromosWithSignInPromos};

  {
    base::HistogramTester histogram_tester;

    SignInWithoutSyncConsent();

    std::vector<FakeControllerInitParams> params;
    params.emplace_back(HISTORY, /*enable_transport_mode=*/true);
    InitializeService(std::move(params));
    base::RunLoop().RunUntilIdle();

    // The signin happened before the SyncService was initialized (this mimics
    // the case where the user previously signed in, and just restarted Chrome),
    // so nothing should be recorded.
    EXPECT_THAT(
        histogram_tester.GetTotalCountsForPrefix("Signin.HistoryOptInState."),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    "Signin.HistoryAlreadyOptedInAccessPoint."),
                IsEmpty());
  }

  {
    base::HistogramTester histogram_tester;
    // Sign out, then back in.
    identity_test_env()->ClearPrimaryAccount();
    SignInWithoutSyncConsent();
    // The histograms are recorded in a posted task.
    base::RunLoop().RunUntilIdle();

    // Now histograms should have been recorded. For `ConsentLevel::kSignin`,
    // history should be off by default.
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Signin.HistoryOptInState.OnSignin"),
        base::BucketsAre(base::Bucket(false, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Signin.HistoryOptInState.OnSync"),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    "Signin.HistoryAlreadyOptedInAccessPoint."),
                IsEmpty());
  }

  {
    base::HistogramTester histogram_tester;
    // Opt in to history.
    service()->GetUserSettings()->SetSelectedType(UserSelectableType::kHistory,
                                                  true);
    ASSERT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
        UserSelectableType::kHistory));

    // Opting in while already signed in should not record the histograms.
    EXPECT_THAT(
        histogram_tester.GetTotalCountsForPrefix("Signin.HistoryOptInState."),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    "Signin.HistoryAlreadyOptedInAccessPoint."),
                IsEmpty());
  }

  {
    base::HistogramTester histogram_tester;
    // Sign out, then back in.
    identity_test_env()->ClearPrimaryAccount();
    SignInWithoutSyncConsent();
    // The histograms are recorded in a posted task.
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
        UserSelectableType::kHistory));

    // Histograms should've been recorded again, and this time the user was
    // already opted in.
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Signin.HistoryOptInState.OnSignin"),
        base::BucketsAre(base::Bucket(true, 1)));
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Signin.HistoryOptInState.OnSync"),
        IsEmpty());
    EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                    "Signin.HistoryAlreadyOptedInAccessPoint."),
                ContainerEq(base::HistogramTester::CountsMap{
                    {"Signin.HistoryAlreadyOptedInAccessPoint.OnSignin", 1}}));
  }
}

TEST_F(SyncServiceImplTest, ShouldRecordHistoryOptInStateOnSync) {
  base::HistogramTester histogram_tester;

  SignInWithSyncConsent();

  std::vector<FakeControllerInitParams> params;
  params.emplace_back(HISTORY, /*enable_transport_mode=*/true);
  InitializeService(std::move(params));
  base::RunLoop().RunUntilIdle();

  // Sync was enabled before the SyncService was initialized (this mimics the
  // case where the user previously enabled sync, and just restarted Chrome), so
  // nothing should be recorded.
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.HistoryOptInState."),
      IsEmpty());
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Signin.HistoryAlreadyOptedInAccessPoint."),
              IsEmpty());

  // Sign out, then back in.
  identity_test_env()->ClearPrimaryAccount();
  SignInWithSyncConsent();
  // The histograms are recorded in a posted task.
  base::RunLoop().RunUntilIdle();

  // Note: In production, enabling sync is a two-step process: First signin
  // with `ConsentLevel::kSignin` (and history sync disabled), then switching
  // to `ConsentLevel::kSync` (with history sync enabled). However,
  // `IdentityTestEnvironment` doesn't faithfully reproduce this process but
  // rather does both steps at once, and so `.OnSignin` gets recorded as "true"
  // here. Rather than adding an inaccurate expectation, let's just verify the
  // total count.
  histogram_tester.ExpectTotalCount("Signin.HistoryOptInState.OnSignin", 1);
  EXPECT_THAT(histogram_tester.GetAllSamples("Signin.HistoryOptInState.OnSync"),
              base::BucketsAre(base::Bucket(true, 1)));
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix(
          "Signin.HistoryAlreadyOptedInAccessPoint."),
      Contains(Pair("Signin.HistoryAlreadyOptedInAccessPoint.OnSync", 1)));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace syncer
