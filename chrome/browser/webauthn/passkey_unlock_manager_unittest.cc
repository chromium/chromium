// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class PasskeyUnlockManagerTest : public testing::Test {
 public:
  PasskeyUnlockManagerTest() = default;
  ~PasskeyUnlockManagerTest() override = default;

 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        PasskeyModelFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<webauthn::TestPasskeyModel>();
        }));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
    profile_ = builder.Build();
    observer_ = std::make_unique<
        testing::StrictMock<MockPasskeyUnlockManagerObserver>>();
    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
    CoreAccountInfo account_info;
    account_info.email = kTestAccount;
    account_info.gaia = GaiaId("gaia");
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service()->SetSignedIn(signin::ConsentLevel::kSignin,
                                     account_info);
    test_sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/true,
        /*types=*/{});
    passkey_unlock_manager_ =
        PasskeyUnlockManagerFactory::GetForProfile(profile_.get());
    passkey_unlock_manager_->AddObserver(observer_.get());
  }

  void TearDown() override {
    passkey_unlock_manager_->RemoveObserver(observer_.get());
    observer_.reset();
    passkey_unlock_manager_ = nullptr;
    test_sync_service_ = nullptr;
    profile_.reset();
  }

  PasskeyUnlockManager* passkey_unlock_manager() {
    return passkey_unlock_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

  testing::StrictMock<MockPasskeyUnlockManagerObserver>& observer() {
    return *observer_;
  }

  TestPasskeyModel* passkey_model() {
    return static_cast<TestPasskeyModel*>(
        PasskeyModelFactory::GetForProfile(profile()));
  }

  syncer::TestSyncService* test_sync_service() { return test_sync_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockErrorUi};
  raw_ptr<PasskeyUnlockManager> passkey_unlock_manager_;
  raw_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::StrictMock<MockPasskeyUnlockManagerObserver>>
      observer_;
};

TEST_F(PasskeyUnlockManagerTest, IsCreated) {
  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

TEST_F(PasskeyUnlockManagerTest, NotifyOnPasskeysChangedWhenPasskeyAdded) {
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiShownWithPasskeysAndActiveSync) {
  // With passkeys and active sync, the manager should notify and the error UI
  // should be shown.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  EXPECT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiHiddenWhenTrustedVaultKeyRequired) {
  // Start with a passkey and active sync.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Passkey unlock error UI should not be shown when trusted vault key is
  // required because that error has a higher priority.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->GetUserSettings()->SetTrustedVaultKeyRequired(true);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiHiddenWhenSyncDisallowed) {
  // Start with a passkey and active sync.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Disallowing sync should cause the error UI to be hidden.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest,
       ErrorUiHiddenWhenTrustedVaultRecoverabilityDegraded) {
  // Start with a passkey and active sync.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Passkey unlock error UI should not be shown when trusted vault
  // recoverability is degraded because that error has a higher priority.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->GetUserSettings()->SetTrustedVaultRecoverabilityDegraded(
      true);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

TEST_F(PasskeyUnlockManagerTest, ErrorUiHiddenWhenPasskeysNotSynced) {
  // Start with a passkey and active sync.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  passkey_model()->AddNewPasskeyForTesting(CreatePasskey());
  ASSERT_TRUE(passkey_unlock_manager()->ShouldDisplayErrorUi());

  // Stopping passkeys sync should cause the error UI to be hidden.
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPreferences});
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(passkey_unlock_manager()->ShouldDisplayErrorUi());
}

}  // namespace

}  // namespace webauthn
