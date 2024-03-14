// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/refresh_token_waiter.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class RefreshTokenWaiterTest : public testing::Test {
 public:
  RefreshTokenWaiterTest() = default;
  RefreshTokenWaiterTest(const RefreshTokenWaiterTest&) = delete;
  RefreshTokenWaiterTest& operator=(const RefreshTokenWaiterTest&) = delete;
  ~RefreshTokenWaiterTest() override = default;

  // Makes the primary account available, which generates a refresh token.
  void MakePrimaryAccountAvailable() {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(&profile_);
    signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  RefreshTokenWaiter refresh_token_waiter_{&profile_};
};

TEST_F(RefreshTokenWaiterTest, TokensAlreadyLoaded) {
  // Make the primary account available, which generates a refresh token.
  MakePrimaryAccountAvailable();

  // Wait for refresh token load.
  bool called = false;
  refresh_token_waiter_.Wait(
      base::BindOnce([](bool* called) { *called = true; }, &called));

  // Callback was called immediately.
  EXPECT_TRUE(called);
}

TEST_F(RefreshTokenWaiterTest, TokensNotLoaded) {
  // Wait for refresh token load.
  bool called = false;
  refresh_token_waiter_.Wait(
      base::BindOnce([](bool* called) { *called = true; }, &called));

  // Callback was not called immediately because tokens are not loaded.
  EXPECT_FALSE(called);

  // Make the primary account available, which generates a refresh token.
  MakePrimaryAccountAvailable();

  // Callback was called.
  EXPECT_TRUE(called);
}

}  // namespace ash
