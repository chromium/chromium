// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_identity_provider_signin_status_context.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/webid/constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::common::webid::LoginStatusAccount;
using blink::common::webid::LoginStatusOptions;

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

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  const url::Origin kIdpOriginA =
      url::Origin::Create(GURL("https://idp.example"));
  const url::Origin kIdpOriginB =
      url::Origin::Create(GURL("https://other-idp.example"));

  const std::string kGivenNameA = "Alice";
  const std::string kPictureUrlA = "https://idp.example/profiles/user.png";
  const LoginStatusAccount kAccountA = LoginStatusAccount(
      /*id=*/"some-identifier-12345",
      /*email=*/"user@idp.example",
      /*name=*/"Alice Smith",
      /*given_name=*/std::make_optional(kGivenNameA),
      /*picture_url=*/std::make_optional(GURL(kPictureUrlA)));

  const std::string kGivenNameB = "Bob";
  const std::string kPictureUrlB =
      "https://other-idp.example/profiles/other-user.png";
  const LoginStatusAccount kAccountB = LoginStatusAccount(
      /*id=*/"some-identifier-12345",
      /*email=*/"other-user@other-idp.example",
      /*name=*/"Bob Smith",
      /*given_name=*/std::make_optional(kGivenNameB),
      /*picture_url=*/std::make_optional(GURL(kPictureUrlB)));

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FederatedIdentityIdentityProviderSigninStatusContext>
      context_;
  TestingProfile profile_;
};

// Test updating the IDP sign-in status.
TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       UpdateSigninMode) {
  // Check unset sign-in status.
  EXPECT_EQ(std::nullopt, context()->GetSigninStatus(kIdpOriginA));

  // Check that setting a sign-in status for a new origin works.
  context()->SetSigninStatus(kIdpOriginA, true, /*options=*/std::nullopt);
  EXPECT_EQ(true, context()->GetSigninStatus(kIdpOriginA));

  // Check that updating the sign-in status works.
  context()->SetSigninStatus(kIdpOriginA, false, /*options=*/std::nullopt);
  EXPECT_EQ(false, context()->GetSigninStatus(kIdpOriginA));
}

// Test storing sign-in statuses for 2 IDPs.
TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest, 2Idps) {
  context()->SetSigninStatus(kIdpOriginA, true, /*options=*/std::nullopt);
  context()->SetSigninStatus(kIdpOriginB, false, /*options=*/std::nullopt);

  EXPECT_EQ(true, context()->GetSigninStatus(kIdpOriginA));
  EXPECT_EQ(false, context()->GetSigninStatus(kIdpOriginB));
}

TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       StoreAndRetrieveLightweightFedCmAccounts) {
  LoginStatusOptions options;
  options.accounts.push_back(kAccountA);

  context()->SetSigninStatus(kIdpOriginA, true, options);
  base::Value::List returned_accounts = context()->GetAccounts(kIdpOriginA);
  EXPECT_EQ(1U, returned_accounts.size());
}

TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       NoAccountsReturnedWhenLoggedOut) {
  LoginStatusOptions options;
  options.accounts.push_back(kAccountA);

  context()->SetSigninStatus(kIdpOriginA, true, std::move(options));
  context()->SetSigninStatus(kIdpOriginA, false, /*options=*/std::nullopt);

  EXPECT_EQ(0U, context()->GetAccounts(kIdpOriginA).size());
}

TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       ExpiredAccountProfilesAreNotReturned) {
  LoginStatusOptions options_a, options_b;

  options_a.accounts.push_back(kAccountA);
  options_a.expiration = base::Seconds(60);

  options_b.accounts.push_back(kAccountB);
  options_b.expiration = base::Seconds(180);

  context()->SetSigninStatus(kIdpOriginA, true, options_a);
  context()->SetSigninStatus(kIdpOriginB, true, options_b);

  task_environment()->FastForwardBy(base::Seconds(61));

  // The accounts should be expired for IdpA, but the login status should've
  // been preserved.
  base::Value::List returned_accounts_idp_a =
      context()->GetAccounts(kIdpOriginA);
  EXPECT_EQ(0U, returned_accounts_idp_a.size());
  EXPECT_TRUE(context()->GetSigninStatus(kIdpOriginA).value_or(false));

  // The accounts should still be valid for IdpB, and the login status should be
  // preserved.
  base::Value::List returned_accounts_idp_b =
      context()->GetAccounts(kIdpOriginB);
  EXPECT_EQ(1U, returned_accounts_idp_b.size());

  base::Value::Dict& account_dict = returned_accounts_idp_b[0].GetDict();
  EXPECT_EQ(*account_dict.FindString(content::webid::kAccountIdKey),
            kAccountB.id);
  EXPECT_EQ(*account_dict.FindString(content::webid::kAccountNameKey),
            kAccountB.name);
  EXPECT_EQ(*account_dict.FindString(content::webid::kAccountEmailKey),
            kAccountB.email);
  EXPECT_TRUE(kAccountB.given_name);
  EXPECT_EQ(*account_dict.FindString(content::webid::kAccountGivenNameKey),
            kAccountB.given_name.value());
  EXPECT_TRUE(kAccountB.picture && !kAccountB.picture->is_empty());
  EXPECT_EQ(*account_dict.FindString(content::webid::kAccountPictureKey),
            kAccountB.picture.value().spec());
  EXPECT_TRUE(context()->GetSigninStatus(kIdpOriginB).value_or(false));
}

TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       ExpiredAccountProfilesClearedByFlush) {
  LoginStatusOptions options_a, options_b;

  options_a.accounts.push_back(kAccountA);
  options_a.expiration = base::Seconds(60);

  options_b.accounts.push_back(kAccountB);
  options_b.expiration = base::Seconds(180);

  context()->SetSigninStatus(kIdpOriginA, true, std::move(options_a));
  context()->SetSigninStatus(kIdpOriginB, true, std::move(options_b));

  task_environment()->FastForwardBy(base::Seconds(61));
  context()->FlushScheduledSaveSettingsCalls();

  auto idp_granted_objects = context()->GetGrantedObjects(kIdpOriginA);
  auto other_idp_granted_objects = context()->GetGrantedObjects(kIdpOriginB);

  // Should be exactly one HostContentSettingsMap entry for each IdP.
  EXPECT_EQ(1U, idp_granted_objects.size());
  EXPECT_EQ(1U, other_idp_granted_objects.size());

  // Make sure that the login statuses were preserved.
  EXPECT_TRUE(idp_granted_objects[0]
                  ->value.FindBool("idp-signin-status")
                  .value_or(false));
  EXPECT_TRUE(other_idp_granted_objects[0]
                  ->value.FindBool("idp-signin-status")
                  .value_or(false));
  // Expired entry should not have an options dictionary, the preserved entry
  // should.
  EXPECT_FALSE(idp_granted_objects[0]->value.FindDict("idp-signin-options"));
  EXPECT_TRUE(
      other_idp_granted_objects[0]->value.FindDict("idp-signin-options"));
}

TEST_F(FederatedIdentityIdentityProviderSigninStatusContextTest,
       SettingOnlyLoginStatusPreservesLightweightFedCmAccounts) {
  LoginStatusOptions options;
  options.accounts.push_back(kAccountA);

  context()->SetSigninStatus(kIdpOriginA, true, options);
  base::Value::List returned_accounts = context()->GetAccounts(kIdpOriginA);
  EXPECT_EQ(1U, returned_accounts.size());

  context()->SetSigninStatus(kIdpOriginA, true, /*options=*/std::nullopt);
  returned_accounts = context()->GetAccounts(kIdpOriginA);
  EXPECT_EQ(1U, returned_accounts.size());
}
