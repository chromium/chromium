// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {

namespace {

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
    profile_ = std::make_unique<TestingProfile>();
    observer_ = std::make_unique<
        testing::StrictMock<MockPasskeyUnlockManagerObserver>>();
    PasskeyModelFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindOnce([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
          return std::make_unique<webauthn::TestPasskeyModel>();
        }));
    passkey_unlock_manager_ =
        PasskeyUnlockManagerFactory::GetForProfile(profile_.get());
    passkey_unlock_manager_->AddObserver(observer_.get());
  }

  void TearDown() override {
    passkey_unlock_manager_->RemoveObserver(observer_.get());
    observer_.reset();
    passkey_unlock_manager_ = nullptr;
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockErrorUi};
  raw_ptr<PasskeyUnlockManager> passkey_unlock_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::StrictMock<MockPasskeyUnlockManagerObserver>>
      observer_;
};

TEST_F(PasskeyUnlockManagerTest, IsCreated) {
  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

TEST_F(PasskeyUnlockManagerTest, NotifyOnPasskeysChanged) {
  EXPECT_CALL(observer(), OnPasskeyUnlockManagerStateChanged());
  sync_pb::WebauthnCredentialSpecifics passkey = CreatePasskey();
  passkey_model()->AddNewPasskeyForTesting(passkey);
}

}  // namespace

}  // namespace webauthn
