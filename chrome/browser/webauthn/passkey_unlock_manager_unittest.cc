// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include <string>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "device/fido/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace webauthn {

namespace {

constexpr char kTestAccount[] = "usertest@gmail.com";

sync_pb::WebauthnCredentialSpecifics CreatePasskey() {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(base::RandBytesAsString(16));
  passkey.set_credential_id(base::RandBytesAsString(16));
  passkey.set_rp_id("abc1.com");
  passkey.set_user_id({1, 2, 3, 4});
  passkey.set_user_name("passkey_username");
  passkey.set_user_display_name("passkey_display_name");
  return passkey;
}

class MockPasskeyUnlockManagerObserver : public PasskeyUnlockManager::Observer {
 public:
  MOCK_METHOD(void, OnPasskeyUnlockManagerStateChanged, (), (override));
  MOCK_METHOD(void, OnPasskeyUnlockManagerShuttingDown, (), (override));
  MOCK_METHOD(void, OnPasskeyUnlockManagerIsReady, (), (override));
};

using ::testing::_;

enum EnclaveManagerStatus { kEnclaveReady, kEnclaveNotReady };
enum PasskeyModelStatus { kPasskeyModelReady, kPasskeyModelNotReady };
using GpmPinStatus = EnclaveManager::GpmPinAvailability;

class PasskeyUnlockManagerTest : public testing::Test {
 protected:
  void ConfigureProfileAndSyncService(
      EnclaveManagerStatus enclave_manager_status,
      PasskeyModelStatus passkey_model_status,
      GpmPinStatus gpm_pin_status) {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        PasskeyModelFactory::GetInstance(),
        base::BindRepeating(&PasskeyUnlockManagerTest::CreateMockPasskeyModel,
                            // `base::Unretained` should be safe because the
                            // test fixture outlives the profile.
                            base::Unretained(this),
                            passkey_model_status == kPasskeyModelReady));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&PasskeyUnlockManagerTest::CreateTestSyncService,
                            // `base::Unretained` should be safe because the
                            // test fixture outlives the profile.
                            base::Unretained(this)));
    builder.AddTestingFactory(
        EnclaveManagerFactory::GetInstance(),
        base::BindRepeating(&PasskeyUnlockManagerTest::CreateMockEnclaveManager,
                            // `base::Unretained` should be safe because the
                            // test fixture outlives the profile.
                            base::Unretained(this), true,
                            enclave_manager_status == kEnclaveReady,
                            gpm_pin_status));
    profile_ = builder.Build();

    CoreAccountInfo account_info;
    account_info.email = kTestAccount;
    account_info.gaia = GaiaId("gaia");
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin,
                                     account_info);
    test_sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/true,
        /*types=*/{});
  }

  std::unique_ptr<KeyedService> CreateMockEnclaveManager(
      bool is_enclave_manager_loaded,
      bool is_enclave_manager_ready,
      GpmPinStatus gpm_pin_status,
      content::BrowserContext* ctx) {
    std::unique_ptr<MockEnclaveManager> enclave_manager_mock =
        std::make_unique<MockEnclaveManager>();
    ON_CALL(*enclave_manager_mock, is_loaded())
        .WillByDefault(testing::Return(is_enclave_manager_loaded));
    ON_CALL(*enclave_manager_mock, is_ready())
        .WillByDefault(testing::Return(is_enclave_manager_ready));
    ON_CALL(*enclave_manager_mock, CheckGpmPinAvailability(_))
        .WillByDefault(
            [gpm_pin_status](
                EnclaveManager::GpmPinAvailabilityCallback callback) {
              std::move(callback).Run(gpm_pin_status);
            });
    EXPECT_CALL(*enclave_manager_mock, CheckGpmPinAvailability(_));
    return enclave_manager_mock;
  }

  std::unique_ptr<KeyedService> CreateMockPasskeyModel(
      bool is_passkey_model_ready,
      content::BrowserContext* ctx) {
    std::unique_ptr<webauthn::TestPasskeyModel> test_passkey_model =
        std::make_unique<webauthn::TestPasskeyModel>();
    test_passkey_model->SetReady(is_passkey_model_ready);
    return test_passkey_model;
  }

  std::unique_ptr<KeyedService> CreateTestSyncService(
      content::BrowserContext* ctx) {
    return std::make_unique<syncer::TestSyncService>();
  }

  void TearDown() override {
    profile_.reset();
  }

  PasskeyUnlockManager* passkey_unlock_manager() {
    return PasskeyUnlockManagerFactory::GetForProfile(profile());
  }

  TestingProfile* profile() { return profile_.get(); }

  TestPasskeyModel* passkey_model() {
    return static_cast<TestPasskeyModel*>(
        PasskeyModelFactory::GetForProfile(profile()));
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  void DisableUVKeySupport() {
    fake_provider_.emplace<crypto::ScopedNullUserVerifyingKeyProvider>();
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void EnableUiExperimentArm(std::string arm_name) {
    feature_params_[device::kPasskeyUnlockErrorUiExperimentArm.name] = arm_name;
    feature_list_.Reset();
    feature_list_.InitAndEnableFeatureWithParameters(
        device::kPasskeyUnlockErrorUi, feature_params_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockManager};
  std::map<std::string, std::string> feature_params_;
  std::unique_ptr<TestingProfile> profile_;
  std::variant<crypto::ScopedFakeUserVerifyingKeyProvider,
               crypto::ScopedNullUserVerifyingKeyProvider,
               crypto::ScopedFailingUserVerifyingKeyProvider>
      fake_provider_;
};

TEST_F(PasskeyUnlockManagerTest, IsCreated) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

TEST_F(PasskeyUnlockManagerTest, NotifyOnPasskeysChangedWhenPasskeyAdded) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);
  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiShownWithPasskeysAndActiveSync) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  // With passkeys and active sync, the manager should notify and the error UI
  // should be shown.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest,
       ErrorUiNotShownWithPasskeysAndActiveSyncWithEnclaveReady) {
  ConfigureProfileAndSyncService(kEnclaveReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiHiddenWhenTrustedVaultKeyRequired) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  // Start with a passkey and active sync.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Passkey unlock error UI should not be shown when trusted vault key is
  // required because that error has a higher priority.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->GetUserSettings()->SetTrustedVaultKeyRequired(true);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiHiddenWhenSyncDisallowed) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  // Start with a passkey and active sync.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Disallowing sync should cause the error UI to be hidden.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest,
       ErrorUiHiddenWhenTrustedVaultRecoverabilityDegraded) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  // Start with a passkey and active sync.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Passkey unlock error UI should not be shown when trusted vault
  // recoverability is degraded because that error has a higher priority.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->GetUserSettings()->SetTrustedVaultRecoverabilityDegraded(
      true);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiHiddenWhenPasskeysNotSynced) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  // Start with a passkey and active sync.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Stopping passkeys sync should cause the error UI to be hidden.
  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

#if BUILDFLAG(IS_CHROMEOS)
// On Chrome OS, AreUserVerifyingKeysSupported always returns true, thus this
// test cannot establish its preconditions.
#define MAYBE_ErrorUiHiddenWithoutUVKeysWithoutGpmPin \
  DISABLED_ErrorUiHiddenWithoutUVKeysWithoutGpmPin
#define MAYBE_ErrorUiVisibleWithoutUVKeysWithGpmPin \
  DISABLED_ErrorUiVisibleWithoutUVKeysWithGpmPin
#else
#define MAYBE_ErrorUiHiddenWithoutUVKeysWithoutGpmPin \
  ErrorUiHiddenWithoutUVKeysWithoutGpmPin
#define MAYBE_ErrorUiVisibleWithoutUVKeysWithGpmPin \
  ErrorUiVisibleWithoutUVKeysWithGpmPin
#endif

TEST_F(PasskeyUnlockManagerTest,
       MAYBE_ErrorUiHiddenWithoutUVKeysWithoutGpmPin) {
  DisableUVKeySupport();
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());

  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest, MAYBE_ErrorUiVisibleWithoutUVKeysWithGpmPin) {
  DisableUVKeySupport();
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinSetAndUsable);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());

  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest, LogsPasskeyCountHistogramWithoutPasskeys) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  // The histogram should be logged on startup even if there are no passkeys.
  base::HistogramTester histogram_tester;

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectUniqueSample("WebAuthentication.PasskeyCount", 0, 1);
}

TEST_F(PasskeyUnlockManagerTest, LogsPasskeyCountHistogramWithPasskeys) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  // The histogram should be logged on startup.
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  base::HistogramTester histogram_tester;

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectUniqueSample("WebAuthentication.PasskeyCount", 1, 1);

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest, TextLablesForDifferentUiExperimentArms) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  EnableUiExperimentArm("text_with_verify_wording");
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_VERIFY));
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_VERIFY));
  EXPECT_EQ(
      passkey_unlock_manager()->GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_VERIFY));

  EnableUiExperimentArm("text_with_get_wording");
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_GET));
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_GET));
  EXPECT_EQ(
      passkey_unlock_manager()->GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_GET));

  EnableUiExperimentArm("text_with_unlock_wording");
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfilePillTitle(),
            l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_PASSKEYS_ERROR_UNLOCK));
  EXPECT_EQ(passkey_unlock_manager()->GetPasskeyErrorProfileMenuDetails(),
            l10n_util::GetStringUTF16(
                IDS_PROFILE_MENU_PASSKEYS_ERROR_DESCRIPTION_UNLOCK));
  EXPECT_EQ(
      passkey_unlock_manager()->GetPasskeyErrorProfileMenuButtonLabel(),
      l10n_util::GetStringUTF16(IDS_PROFILE_MENU_PASSKEYS_ERROR_BUTTON_UNLOCK));
}

TEST_F(PasskeyUnlockManagerTest,
       LogsPasskeyReadinessHistogramWhenPasskeysReady) {
  ConfigureProfileAndSyncService(kEnclaveReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  base::HistogramTester histogram_tester;

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectBucketCount("WebAuthentication.PasskeyReadiness", true,
                                     1);
}

TEST_F(PasskeyUnlockManagerTest,
       LogsPasskeyReadinessHistogramWhenPasskeysLocked) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelReady,
                                 GpmPinStatus::kGpmPinUnset);

  base::HistogramTester histogram_tester;

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectBucketCount("WebAuthentication.PasskeyReadiness",
                                     false, 1);
}

TEST_F(PasskeyUnlockManagerTest,
       LogsPasskeyCountHistogramWhenPasskeyModelReady) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelNotReady,
                                 GpmPinStatus::kGpmPinUnset);
  testing::StrictMock<MockPasskeyUnlockManagerObserver> observer =
      testing::StrictMock<MockPasskeyUnlockManagerObserver>();
  passkey_unlock_manager()->AddObserver(&observer);

  EXPECT_CALL(observer, OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  base::HistogramTester histogram_tester;

  passkey_model()->SetReady(true);
  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectUniqueSample("WebAuthentication.PasskeyCount", 1, 1);

  passkey_unlock_manager()->RemoveObserver(&observer);
}

TEST_F(PasskeyUnlockManagerTest,
       LogsGpmPinStatusHistogramWhenGpmPinStatusUnset) {
  ConfigureProfileAndSyncService(kEnclaveNotReady, kPasskeyModelNotReady,
                                 GpmPinStatus::kGpmPinUnset);
  base::HistogramTester histogram_tester;

  AdvanceClock(base::Seconds(31));
  histogram_tester.ExpectUniqueSample("WebAuthentication.GpmPinStatus",
                                      GpmPinStatus::kGpmPinUnset, 1);
}

}  // namespace

}  // namespace webauthn
