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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/service/data_type_manager_impl.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/test/fake_data_type_controller.h"
#include "components/sync/test/fake_sync_api_component_factory.h"
#include "components/sync/test/fake_sync_engine.h"
#include "components/sync/test/sync_client_mock.h"
#include "components/sync/test/sync_service_impl_bundle.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::AtLeast;
using testing::ByMove;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::Return;

namespace syncer {

namespace {

MATCHER_P(ContainsDataType, type, "") {
  return arg.Has(type);
}

constexpr char kTestUser[] = "test_user@gmail.com";

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
    ShutdownAndDeleteService();
  }

  void SignInWithoutSyncConsent() {
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSignin);
  }

  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  void SignInWithSyncConsent() {
    identity_test_env()->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSync);
  }

  void InitializeService(std::vector<std::pair<ModelType, bool>>
                             registered_types_and_transport_mode_support = {
                                 {BOOKMARKS, false},
                                 {DEVICE_INFO, true}}) {
    DCHECK(!service_);

    // Default includes a regular controller and a transport-mode controller.
    DataTypeController::TypeVector controllers;
    for (const auto& [type, transport_mode_support] :
         registered_types_and_transport_mode_support) {
      auto controller = std::make_unique<FakeDataTypeController>(
          type, transport_mode_support);
      // Hold a raw pointer to directly interact with the controller.
      controller_map_[type] = controller.get();
      controllers.push_back(std::move(controller));
    }

    std::unique_ptr<SyncClientMock> sync_client =
        sync_service_impl_bundle_.CreateSyncClientMock();
    sync_client_ = sync_client.get();
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    service_ = std::make_unique<SyncServiceImpl>(
        sync_service_impl_bundle_.CreateBasicInitParams(
            std::move(sync_client)));
    service_->Initialize();
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
    ON_CALL(*sync_client, CreateDataTypeControllers)
        .WillByDefault(Return(ByMove(std::move(controllers))));

    SyncServiceImpl::InitParams init_params =
        sync_service_impl_bundle_.CreateBasicInitParams(std::move(sync_client));

    prefs()->SetBoolean(prefs::kEnableLocalSyncBackend, true);
    init_params.identity_manager = nullptr;

    service_ = std::make_unique<SyncServiceImpl>(std::move(init_params));
    service_->Initialize();
  }

  void ShutdownAndDeleteService() {
    if (service_) {
      service_->Shutdown();
    }
    service_.reset();
  }

  void PopulatePrefsForInitialSyncFeatureSetupComplete() {
    CHECK(!service_);
    component_factory()->set_first_time_sync_configure_done(true);
    // Set first sync time before initialize to simulate a complete sync setup.
    SyncPrefs sync_prefs(prefs());
    sync_prefs.SetSelectedTypes(
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

  TestingPrefServiceSimple* prefs() {
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

  MockSyncInvalidationsService* sync_invalidations_service() {
    return sync_service_impl_bundle_.sync_invalidations_service();
  }

  trusted_vault::FakeTrustedVaultClient* trusted_vault_client() {
    return sync_service_impl_bundle_.trusted_vault_client();
  }

  FakeDataTypeController* get_controller(ModelType type) {
    return controller_map_[type];
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  SyncServiceImplBundle sync_service_impl_bundle_;
  std::unique_ptr<SyncServiceImpl> service_;
  raw_ptr<SyncClientMock, DanglingUntriaged> sync_client_ =
      nullptr;  // Owned by |service_|.
  // The controllers are owned by |service_|.
  std::map<ModelType, FakeDataTypeController*> controller_map_;
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

  EXPECT_TRUE(service()->GetDisableReasons().Empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
}

TEST_F(SyncServiceImplTest, SuccessfulLocalBackendInitialization) {
  InitializeServiceWithLocalSyncBackend();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().Empty());
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
  sync_prefs.SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());

  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().Empty());

  // Sync should immediately start up in transport mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
}
#endif

TEST_F(SyncServiceImplTest, ModelTypesForTransportMode) {
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

  // ModelTypes for sync-the-feature are not configured.
  EXPECT_FALSE(service()->GetActiveDataTypes().Has(BOOKMARKS));

  // ModelTypes for sync-the-transport are configured.
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
  EXPECT_TRUE(service()->GetDisableReasons().Empty());

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
  EXPECT_TRUE(service()->GetDisableReasons().Empty());
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
  component_factory()->AllowFakeEngineInitCompletion(false);
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  ShutdownAndDeleteService();
}

// Certain SyncServiceImpl tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out".
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Test the user signing out before the backend's initialization completes.
TEST_F(SyncServiceImplTest, EarlySignOut) {
  // Set up a fake sync engine that will not immediately finish initialization.
  component_factory()->AllowFakeEngineInitCompletion(false);
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
  account_mutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);
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
  account_mutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);
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
  ASSERT_TRUE(component_factory()->HasTransportDataIncludingFirstSync());

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();
  // These are specific to sync-the-feature and should be cleared.
  EXPECT_FALSE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_NOT_SIGNED_IN}),
            service()->GetDisableReasons());
  EXPECT_FALSE(component_factory()->HasTransportDataIncludingFirstSync());
#if BUILDFLAG(IS_IOS)
  SyncPrefs sync_prefs(prefs());
  EXPECT_FALSE(
      sync_prefs.IsOptedInForBookmarksAndReadingListAccountStorageForTesting());
#endif  // BUILDFLAG(IS_IOS)
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

#if BUILDFLAG(IS_IOS)
  // Opt in bookmarks and reading list account storage.
  SyncPrefs sync_prefs(prefs());
  sync_prefs.SetBookmarksAndReadingListAccountStorageOptIn(true);
#endif  // BUILDFLAG(IS_IOS)

  // Sign-out.
  signin::PrimaryAccountMutator* account_mutator =
      identity_manager()->GetPrimaryAccountMutator();
  DCHECK(account_mutator) << "Account mutator should only be null on ChromeOS.";
  account_mutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  // Wait for SyncServiceImpl to be notified.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(component_factory()->HasTransportDataIncludingFirstSync());
#if BUILDFLAG(IS_IOS)
  EXPECT_FALSE(
      sync_prefs.IsOptedInForBookmarksAndReadingListAccountStorageForTesting());
#endif  // BUILDFLAG(IS_IOS)
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
  // To disable addresses sync, the kAutofill selectable type will be disabled.
  // This will also disable the kPayments selectable type. Therefore,
  // AUTOFILL_WALLET_DATA should be registered, since
  // SyncUserSettingsImpl::SetSelectedType() (which is used to disable both
  // selectable types) checks whether at least one of the data types mapped to
  // the disabled selectable type is registered.
  InitializeService({{CONTACT_INFO, true}, {AUTOFILL_WALLET_DATA, true}});

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
  // kPayments should be disabled when kAutofill is disabled.
  // TODO(crbug.com/1435431): It shouldn't be disabled once kPayments is
  // decoupled from kAutofill.
  if (!base::FeatureList::IsEnabled(kSyncDecoupleAddressPaymentSettings)) {
    EXPECT_FALSE(service()->GetUserSettings()->GetSelectedTypes().Has(
        UserSelectableType::kPayments));
  }

  // The user enables addresses sync.
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, true);
  // TODO(crbug.com/1435431): This should be removed once kPayments is decoupled
  // from kAutofill.
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, true);

  // UserSelectableType::kAutofill should have been enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));
  // TODO(crbug.com/1435431): This should be removed once kPayments is decoupled
  // from kAutofill.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kPayments));

  // This call represents the passphrase type being determined again after a
  // browser restart.
  service()->PassphraseTypeChanged(PassphraseType::kCustomPassphrase);

  // UserSelectableType::kAutofill should stay enabled.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kAutofill));
  // TODO(crbug.com/1435431): This should be removed once kPayments is decoupled
  // from kAutofill.
  EXPECT_TRUE(service()->GetUserSettings()->GetSelectedTypes().Has(
      UserSelectableType::kPayments));
}

TEST_F(
    SyncServiceImplTest,
    AddressesSyncShouldNotBeDisabledForSignedInUsersWithNewlyCustomPassphraseSet) {
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
  // To disable addresses sync, the kAutofill selectable type will be disabled.
  // This will also disable the kPayments selectable type. Therefore,
  // AUTOFILL_WALLET_DATA should be registered, since
  // SyncUserSettingsImpl::SetSelectedType() (which is used to disable both
  // selectable types) checks whether at least one of the data types mapped to
  // the disabled selectable type is registered.
  InitializeService({{CONTACT_INFO, true}, {AUTOFILL_WALLET_DATA, true}});

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

  // TODO(crbug.com/1462552): Update once kSync becomes unreachable or is
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

  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
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

  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
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

  account_mutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::kTest,
      signin_metrics::SignoutDelete::kIgnoreMetric);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

TEST_F(SyncServiceImplTest, StopAndClearWillClearDataAndSwitchToTransportMode) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_TRUE(component_factory()->HasTransportDataIncludingFirstSync());

  service()->StopAndClear();

  EXPECT_FALSE(component_factory()->HasTransportDataIncludingFirstSync());

  // Even though Sync-the-feature is disabled, there's still an (unconsented)
  // signed-in account, so Sync-the-transport should still be running.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, sync-the-feature remains on. Note however that this is not a
  // common scenario, because in most case StopAndClear() would be issued from
  // a codepath that would prevent either sync-the-feature (e.g. dashboard
  // reset) or sync-the-transport (e.g. unrecoverable error) from starting.
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
  EXPECT_TRUE(service()->IsSyncFeatureActive());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  // Except for Ash, StopAndClear() turns sync-the-feature off because
  // IsInitialSyncFeatureSetupComplete() becomes false.
  EXPECT_FALSE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  EXPECT_FALSE(service()->IsSyncFeatureEnabled());
  EXPECT_FALSE(service()->IsSyncFeatureActive());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Verify that sync transport data is cleared when the service is initializing
// and account is signed out.
// This code path doesn't exist on ChromeOS-Ash, since signout is not possible.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncServiceImplTest, ClearTransportDataOnInitializeWhenSignedOut) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();

  // Clearing prefs can be triggered only after `IdentityManager` finishes
  // loading the list of accounts, so wait for it to complete.
  identity_test_env()->WaitForRefreshTokensLoaded();

  ASSERT_TRUE(component_factory()->HasTransportDataIncludingFirstSync());

  // Don't sign-in before creating the service.
  InitializeService();

  EXPECT_FALSE(component_factory()->HasTransportDataIncludingFirstSync());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplTest, StopSyncAndClearTwiceDoesNotCrash) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Disable sync.
  service()->StopAndClear();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, sync-the-feature remains on. Note however that this is not a
  // common scenario, because in most case StopAndClear() would be issued from
  // a codepath that would prevent either sync-the-feature (e.g. dashboard
  // reset) or sync-the-transport (e.g. unrecoverable error) from starting.
  ASSERT_TRUE(service()->IsSyncFeatureEnabled());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  // Except for Ash, StopAndClear() turns sync-the-feature off because
  // IsInitialSyncFeatureSetupComplete() becomes false.
  ASSERT_FALSE(
      service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(service()->IsSyncFeatureEnabled());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Calling StopAndClear while already stopped should not crash. This may
  // (under some circumstances) happen when the user enables sync again but hits
  // the cancel button at the end of the process.
  service()->StopAndClear();
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

  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
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

  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
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
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());
  ASSERT_EQ(service()->GetActiveDataTypes(),
            ModelTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}));

  SyncProtocolError client_cmd;
  client_cmd.action = RESET_LOCAL_SYNC_DATA;
  service()->OnActionableProtocolError(client_cmd);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().Empty());
  EXPECT_TRUE(service()->IsSyncFeatureEnabled());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(service()->GetActiveDataTypes(),
            ModelTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}));
  EXPECT_GT(get_controller(BOOKMARKS)->model()->clear_metadata_call_count(), 0);
}

// Test that when SyncServiceImpl receives actionable error
// DISABLE_SYNC_ON_CLIENT it disables sync and signs out.
TEST_F(SyncServiceImplTest, DisableSyncOnClient) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  ASSERT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(
      service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // TODO(crbug.com/1462552): Update once kSync becomes unreachable or is
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ash does not support signout.
  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(service()->GetDisableReasons().Empty());
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
  // TODO(crbug.com/1462552): Remove once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(service()->GetDisableReasons().Empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // TrustedVault data should have been cleared.
  EXPECT_THAT(trusted_vault_client()->GetStoredKeys(primary_account_gaia_id),
              IsEmpty());

  EXPECT_GT(get_controller(BOOKMARKS)->model()->clear_metadata_call_count(), 0);

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

// Verify a that local sync mode isn't impacted by sync being disabled.
TEST_F(SyncServiceImplTest, LocalBackendUnimpactedByPolicy) {
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));
  InitializeServiceWithLocalSyncBackend();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetDisableReasons().Empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // The transport should continue active even if kSyncManaged becomes true.
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));

  EXPECT_TRUE(service()->GetDisableReasons().Empty());
  EXPECT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());

  // Setting kSyncManaged back to false should also make no difference.
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(false));

  EXPECT_TRUE(service()->GetDisableReasons().Empty());
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
  EXPECT_EQ(CONFIGURE_REASON_NEW_CLIENT,
            data_type_manager()->last_configure_reason_for_test());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            data_type_manager()->last_configure_reason_for_test());
  ShutdownAndDeleteService();

  // Nth sync.
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,
            data_type_manager()->last_configure_reason_for_test());

  // Reconfiguration.
  // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and immediately
  // releasing it again (via the temporary unique_ptr going away).
  service()->GetSetupInProgressHandle();
  EXPECT_EQ(CONFIGURE_REASON_RECONFIGURATION,
            data_type_manager()->last_configure_reason_for_test());
  ShutdownAndDeleteService();
}

// Regression test for crbug.com/1043642, can be removed once
// SyncServiceImpl usages after shutdown are addressed.
TEST_F(SyncServiceImplTest, ShouldProvideDisableReasonsAfterShutdown) {
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  service()->Shutdown();
  EXPECT_FALSE(service()->GetDisableReasons().Empty());
}

TEST_F(SyncServiceImplTest, ShouldSendDataTypesToSyncInvalidationsService) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService(
      /*registered_types_and_transport_mode_support=*/
      {
          {BOOKMARKS, false},
          {DEVICE_INFO, true},
      });
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
  InitializeService(
      /*registered_types_and_transport_mode_support=*/
      {
          {BOOKMARKS, false},
          {DEVICE_INFO, true},
      });

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
  InitializeService(
      /*registered_types_and_transport_mode_support=*/
      {
          {BOOKMARKS, false},
          {DEVICE_INFO, true},
      });
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
  InitializeService({{SESSIONS, false}, {BOOKMARKS, false}});
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
  InitializeService(
      /*registered_types_and_transport_mode_support=*/
      {
          {BOOKMARKS, false},
          {DEVICE_INFO, true},
      });
  get_controller(BOOKMARKS)
      ->model()
      ->EnableSkipEngineConnectionForActivationResponse();

  EXPECT_CALL(*sync_invalidations_service(),
              SetInterestedDataTypes(AllOf(ContainsDataType(DEVICE_INFO),
                                           Not(ContainsDataType(BOOKMARKS)))));
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
  ShutdownAndDeleteService();
}

TEST_F(SyncServiceImplTest,
       ShouldStopListeningPermanentlyOnDisableSyncAndClearData) {
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*sync_invalidations_service(), StopListeningPermanently());
  service()->StopAndClear();
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

  EXPECT_EQ(0, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());
  // Clearing metadata should work even if the engine is not running.
  service()->StopAndClear();
  EXPECT_EQ(1, get_controller(BOOKMARKS)->model()->clear_metadata_call_count());
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorDownloadStatus) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  data_type_manager()->OnSingleDataTypeWillStop(
      syncer::BOOKMARKS,
      SyncError(FROM_HERE, SyncError::ErrorType::DATATYPE_ERROR,
                "Data type failure", syncer::BOOKMARKS));
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kError);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorDownloadStatusWhenSyncDisabled) {
  base::HistogramTester histogram_tester;

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  // OnInvalidationStatusChanged() is used to only notify observers. This will
  // cause the histogram recorder to check data types status.
  service()->OnInvalidationStatusChanged();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kError);
  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime",
                                    /*expected_count=*/0);
}

TEST_F(SyncServiceImplTest, ShouldReturnWaitingDownloadStatus) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  ASSERT_THAT(data_type_manager(), IsNull());

  bool met_configuring_data_type_manager = false;
  testing::NiceMock<MockSyncServiceObserver> mock_sync_service_observer;
  ON_CALL(mock_sync_service_observer, OnStateChanged)
      .WillByDefault(Invoke([this, &met_configuring_data_type_manager](
                                SyncService* service) {
        EXPECT_NE(service->GetDownloadStatusFor(syncer::BOOKMARKS),
                  SyncService::ModelTypeDownloadStatus::kError);
        if (!data_type_manager()) {
          return;
        }
        if (data_type_manager()->state() ==
            DataTypeManager::State::CONFIGURING) {
          met_configuring_data_type_manager = true;
          EXPECT_EQ(service->GetDownloadStatusFor(syncer::BOOKMARKS),
                    SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);
        }
      }));

  // Observers must be added after initialization has been started.
  ASSERT_THAT(engine(), IsNull());

  // GetDownloadStatusFor() must be called only after Initialize(), see
  // SyncServiceImpl::Initialize().
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

  service()->AddObserver(&mock_sync_service_observer);
  base::RunLoop().RunUntilIdle();
  SetInvalidationsEnabled();

  EXPECT_TRUE(met_configuring_data_type_manager);
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kUpToDate);
  service()->RemoveObserver(&mock_sync_service_observer);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorWhenDataTypeDisabled) {
  base::HistogramTester histogram_tester;

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
            SyncService::ModelTypeDownloadStatus::kError);

  // Finish initialization and double check that the status hasn't changed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kError);

  SetInvalidationsEnabled();
  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime.BOOKMARK",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime",
                                    /*expected_count=*/1);
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
            SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::DEVICE_INFO),
            SyncService::ModelTypeDownloadStatus::kUpToDate);
}

TEST_F(SyncServiceImplTest, ShouldWaitForInitializedInvalidations) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

  SetInvalidationsEnabled();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kUpToDate);
}

TEST_F(SyncServiceImplTest, ShouldWaitForPollRequest) {
  base::HistogramTester histogram_tester;

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();
  base::RunLoop().RunUntilIdle();

  SetInvalidationsEnabled();

  ASSERT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kUpToDate);

  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime.BOOKMARK",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime",
                                    /*expected_count=*/1);

  // OnInvalidationStatusChanged() is used to only notify observers, this is
  // required for metrics since they are calculated only when SyncService state
  // changes.
  engine()->SetPollIntervalElapsed(true);
  service()->OnInvalidationStatusChanged();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

  engine()->SetPollIntervalElapsed(false);
  service()->OnInvalidationStatusChanged();
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kUpToDate);

  // The histograms should be recorded only once.
  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime.BOOKMARK",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Sync.ModelTypeUpToDateTime",
                                    /*expected_count=*/1);

  // Ignore following poll requests once the first sync cycle is completed.
  service()->OnSyncCycleCompleted(MakeDefaultSyncCycleSnapshot());
  engine()->SetPollIntervalElapsed(true);
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kUpToDate);
}

TEST_F(SyncServiceImplTest, ShouldReturnErrorOnSyncPaused) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  ASSERT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kWaitingForUpdates);

  // Mimic entering Sync paused state.
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  ASSERT_EQ(SyncService::TransportState::PAUSED,
            service()->GetTransportState());

  // Expect the error status when Sync is paused.
  EXPECT_EQ(service()->GetDownloadStatusFor(syncer::BOOKMARKS),
            SyncService::ModelTypeDownloadStatus::kError);
}

// These tests cover signing in after browser startup, which isn't supported on
// ChromeOS-Ash (where there's always a signed-in user).
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(
    SyncServiceImplTest,
    GetTypesWithPendingDownloadForInitialSyncDuringFirstSyncInTransportMode) {
  base::test::ScopedFeatureList feature_list(kEnableBookmarksAccountStorage);

  component_factory()->AllowFakeEngineInitCompletion(false);
  InitializeService(
      /*registered_types_and_transport_mode_support=*/
      {
          {BOOKMARKS, true},
          {DEVICE_INFO, true},
      });
  base::RunLoop().RunUntilIdle();

#if BUILDFLAG(IS_IOS)
  // Outside iOS, transport mode considers all types as enabled by default. On
  // iOS, for BOOKMARKS to be listed as preferred, an explicit API call is
  // needed.
  service()->GetUserSettings()->SetBookmarksAndReadingListAccountStorageOptIn(
      true);
#endif  // BUILDFLAG(IS_IOS)

  SignInWithoutSyncConsent();

  ASSERT_EQ(SyncService::TransportState::START_DEFERRED,
            service()->GetTransportState());

  // START_DEFERRED is very short-lived upon sign-in, so it doesn't matter
  // much what the API returns (added here for documentation purposes).
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // During first-sync INITIALIZING, all preferred datatypes are listed, which
  // in this test fixture means NIGORI, BOOKMARKS and DEVICE_INFO.
  EXPECT_EQ(ModelTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}),
            service()->GetTypesWithPendingDownloadForInitialSync());

  // Once fully initialized, it is delegated to DataTypeManager.
  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());
}

TEST_F(SyncServiceImplTest,
       GetTypesWithPendingDownloadForInitialSyncDuringFirstSync) {
  component_factory()->AllowFakeEngineInitCompletion(false);
  InitializeService();
  base::RunLoop().RunUntilIdle();
  SignInWithSyncConsent();

  service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  ASSERT_EQ(SyncService::TransportState::START_DEFERRED,
            service()->GetTransportState());

  // START_DEFERRED is very short-lived upon sign-in, so it doesn't matter
  // much what the API returns (added here for documentation purposes).
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // During first-sync INITIALIZING, all preferred datatypes are listed, which
  // in this test fixture means NIGORI, BOOKMARKS and DEVICE_INFO.
  EXPECT_EQ(ModelTypeSet({NIGORI, BOOKMARKS, DEVICE_INFO}),
            service()->GetTypesWithPendingDownloadForInitialSync());

  // Once fully initialized, it is delegated to DataTypeManager.
  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncServiceImplTest,
       GetTypesWithPendingDownloadForInitialSyncDuringNthSync) {
  component_factory()->AllowFakeEngineInitCompletion(false);

  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  InitializeService();

  ASSERT_EQ(SyncService::TransportState::START_DEFERRED,
            service()->GetTransportState());

  // During non-first-sync initialization, usually during profile startup,
  // SyncService doesn't actually know which datatypes are pending download, so
  // it defaults to returning an empty set.
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(SyncService::TransportState::INITIALIZING,
            service()->GetTransportState());

  // Same as above.
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());

  // Once fully initialized, it is delegated to DataTypeManager.
  engine()->TriggerInitializationCompletion(/*success=*/true);
  ASSERT_EQ(SyncService::TransportState::ACTIVE,
            service()->GetTransportState());
  EXPECT_EQ(ModelTypeSet(),
            service()->GetTypesWithPendingDownloadForInitialSync());
}

TEST_F(SyncServiceImplTest, EarlyCallToGetTypesWithUnsyncedDataShouldNotCrash) {
  InitializeService();
  base::MockCallback<base::OnceCallback<void(ModelTypeSet)>> cb;
  EXPECT_CALL(cb, Run(ModelTypeSet()));
  service()->GetTypesWithUnsyncedData(cb.Get());
}

TEST_F(SyncServiceImplTest,
       ShouldOnlyForwardEnabledTypesToSyncClientUponGetLocalDataDescriptions) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  // Only PASSWORDS datatype is enabled.
  InitializeService({{PASSWORDS, true}});
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(service()->GetActiveDataTypes(), ModelTypeSet({NIGORI, PASSWORDS}));

  // PASSWORDS and BOOKMARKS is queried from the sync service.
  ModelTypeSet requested_types{PASSWORDS, BOOKMARKS};
  // Only PASSWORDS datatype is queried from the sync client.
  EXPECT_CALL(*sync_client(),
              GetLocalDataDescriptions(ModelTypeSet{PASSWORDS}, ::testing::_));

  service()->GetLocalDataDescriptions(requested_types, base::DoNothing());
}

TEST_F(SyncServiceImplTest,
       ShouldNotForwardToSyncClientUponGetLocalDataDescriptionsIfSyncDisabled) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();
  InitializeService({{PASSWORDS, true}, {BOOKMARKS, true}});
  base::RunLoop().RunUntilIdle();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // PASSWORDS and BOOKMARKS is queried from the sync service.
  ModelTypeSet requested_types{PASSWORDS, BOOKMARKS};
  // No query to the sync client.
  EXPECT_CALL(*sync_client(),
              GetLocalDataDescriptions(ModelTypeSet{}, ::testing::_))
      .Times(0);

  service()->GetLocalDataDescriptions(requested_types, base::DoNothing());
}

TEST_F(SyncServiceImplTest,
       ShouldOnlyForwardEnabledTypesToSyncClientUponTriggerLocalDataMigration) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  SignInWithSyncConsent();
  // Only PASSWORDS datatype is enabled.
  InitializeService({{PASSWORDS, true}});
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(service()->GetActiveDataTypes(), ModelTypeSet({NIGORI, PASSWORDS}));

  // PASSWORDS and BOOKMARKS is queried from the sync service.
  ModelTypeSet requested_types{PASSWORDS, BOOKMARKS};
  // Only PASSWORDS datatype is queried from the sync client.
  EXPECT_CALL(*sync_client(),
              TriggerLocalDataMigration(ModelTypeSet{PASSWORDS}));

  service()->TriggerLocalDataMigration(ModelTypeSet{PASSWORDS, BOOKMARKS});
}

TEST_F(
    SyncServiceImplTest,
    ShouldNotForwardToSyncClientUponTriggerLocalDataMigrationIfSyncDisabled) {
  PopulatePrefsForInitialSyncFeatureSetupComplete();
  prefs()->SetManagedPref(prefs::internal::kSyncManaged, base::Value(true));
  SignInWithSyncConsent();
  InitializeService({{PASSWORDS, true}, {BOOKMARKS, true}});
  base::RunLoop().RunUntilIdle();

  // Sync was disabled due to the policy.
  EXPECT_EQ(SyncService::DisableReasonSet(
                {SyncService::DISABLE_REASON_ENTERPRISE_POLICY}),
            service()->GetDisableReasons());
  EXPECT_EQ(SyncService::TransportState::DISABLED,
            service()->GetTransportState());

  // PASSWORDS and BOOKMARKS is queried from the sync service.
  ModelTypeSet requested_types{PASSWORDS, BOOKMARKS};
  // No query to the sync client.
  EXPECT_CALL(*sync_client(), TriggerLocalDataMigration(ModelTypeSet{}))
      .Times(0);

  service()->TriggerLocalDataMigration(requested_types);
}

}  // namespace
}  // namespace syncer
