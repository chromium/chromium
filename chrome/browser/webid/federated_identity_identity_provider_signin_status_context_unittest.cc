// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_identity_provider_signin_status_context.h"

#include <memory>

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

class FederatedIdentityIdentityProviderSigninStatusContextTest
    : public testing::Test {
 public:
  FederatedIdentityIdentityProviderSigninStatusContextTest() {
    context_ =
        std::make_unique<FederatedIdentityIdentityProviderSigninStatusContext>(
            &profile_);
  }

  void TearDown() override { context_.reset(); }

  ~FederatedIdentityIdentityProviderSigninStatusContextTest() override =
      default;

  FederatedIdentityIdentityProviderSigninStatusContextTest(
      FederatedIdentityIdentityProviderSigninStatusContextTest&) = delete;
  FederatedIdentityIdentityProviderSigninStatusContextTest& operator=(
      FederatedIdentityIdentityProviderSigninStatusContextTest&) = delete;

  FederatedIdentityIdentityProviderSigninStatusContext* context() {
    return context_.get();
  }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FederatedIdentityIdentityProviderSigninStatusContext>
      context_;
  TestingProfile profile_;
};

// Test updating the IDP sign-in status.
TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       UpdateSigninMode) {
  const url::Origin idp = url::Origin::Create(GURL("https://idp.example"));

  // Check unset sign-in status.
  EXPECT_EQ(std::nullopt, context()->GetSigninStatus(idp));

  // Check that setting a sign-in status for a new origin works.
  context()->SetSigninStatus(idp, true);
  EXPECT_EQ(true, context()->GetSigninStatus(idp));

  // Check that updating the sign-in status works.
  context()->SetSigninStatus(idp, false);
  EXPECT_EQ(false, context()->GetSigninStatus(idp));
}

// Test storing sign-in statuses for 2 IDPs.
TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest, 2Idps) {
  const url::Origin idp1 = url::Origin::Create(GURL("https://idp1.example"));
  const url::Origin idp2 = url::Origin::Create(GURL("https://idp2.example"));

  context()->SetSigninStatus(idp1, true);
  context()->SetSigninStatus(idp2, false);

  EXPECT_EQ(true, context()->GetSigninStatus(idp1));
  EXPECT_EQ(false, context()->GetSigninStatus(idp2));
}
