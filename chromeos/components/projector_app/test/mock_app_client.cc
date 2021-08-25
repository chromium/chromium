// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/test/mock_app_client.h"

#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {
const char kTestUserEmail[] = "testuser1@gmail.com";
}  // namespace

namespace chromeos {

MockAppClient::MockAppClient() {
  identity_test_environment_.MakePrimaryAccountAvailable(
      kTestUserEmail, signin::ConsentLevel::kSignin);
  identity_test_environment_.SetRefreshTokenForPrimaryAccount();
}

MockAppClient::~MockAppClient() = default;

signin::IdentityManager* MockAppClient::GetIdentityManager() {
  return identity_test_environment_.identity_manager();
}

void MockAppClient::SetAutomaticIssueOfAccessTokens(bool success) {
  identity_test_environment_.SetAutomaticIssueOfAccessTokens(success);
}

void MockAppClient::WaitForAccessRequest(const std::string& account_email) {
  GrantOAuthTokenFor(account_email, base::Time::Now());
}

void MockAppClient::GrantOAuthTokenFor(const std::string& account_email,
                                       const base::Time& expiry_time) {
  const auto& core_account_id =
      GetIdentityManager()
          ->FindExtendedAccountInfoByEmailAddress(account_email)
          .account_id;
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          core_account_id, "validToken", expiry_time);
}

void MockAppClient::AddSecondaryAccount(const std::string& account_email) {
  const auto& account_info =
      identity_test_environment_.MakeAccountAvailable(account_email);
  identity_test_environment_.SetRefreshTokenForAccount(account_info.account_id);
}

}  // namespace chromeos
