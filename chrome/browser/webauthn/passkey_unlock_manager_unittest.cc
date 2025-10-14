// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {

namespace {

class PasskeyUnlockManagerTest : public testing::Test {
 public:
  PasskeyUnlockManagerTest() = default;
  ~PasskeyUnlockManagerTest() override = default;

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    passkey_unlock_manager_ =
        PasskeyUnlockManagerFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    passkey_unlock_manager_ = nullptr;
    profile_.reset();
  }

  PasskeyUnlockManager* passkey_unlock_manager() {
    return passkey_unlock_manager_;
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockErrorUi};
  raw_ptr<PasskeyUnlockManager> passkey_unlock_manager_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(PasskeyUnlockManagerTest, IsCreated) {
  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

}  // namespace

}  // namespace webauthn
