// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/load_credentials_state.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kAccountEmail[] = "user @gmail.com ";

namespace {

class DiagnosticsProviderTest : public testing::Test {
 public:
  DiagnosticsProviderTest() {
    identity_test_env()->WaitForRefreshTokensLoaded();
  }

  DiagnosticsProviderTest(const DiagnosticsProviderTest&) = delete;
  DiagnosticsProviderTest& operator=(const DiagnosticsProviderTest&) = delete;

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::DiagnosticsProvider* diagnostics_provider() {
    return identity_test_env_.identity_manager()->GetDiagnosticsProvider();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  signin::IdentityTestEnvironment identity_test_env_;
};

}  // namespace

TEST_F(DiagnosticsProviderTest, Basic) {
  // Accessing the DiagnosticProvider should not crash.
  ASSERT_TRUE(identity_test_env()->identity_manager());
  EXPECT_TRUE(
      identity_test_env()->identity_manager()->GetDiagnosticsProvider());
}

TEST_F(DiagnosticsProviderTest, GetDetailedStateOfLoadingOfRefreshTokens) {
  EXPECT_EQ(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
      diagnostics_provider()->GetDetailedStateOfLoadingOfRefreshTokens());
}

TEST_F(DiagnosticsProviderTest, GetDelayBeforeMakingAccessTokenRequests) {
  base::TimeDelta zero;
  EXPECT_EQ(diagnostics_provider()->GetDelayBeforeMakingAccessTokenRequests(),
            zero);
  CoreAccountId account_id =
      identity_test_env()->MakeAccountAvailable(kAccountEmail).account_id;
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id, GoogleServiceAuthError(
                      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE));
  EXPECT_GT(diagnostics_provider()->GetDelayBeforeMakingAccessTokenRequests(),
            zero);
}

TEST_F(DiagnosticsProviderTest, GetDelayBeforeMakingCookieRequests) {
  base::TimeDelta zero;
  identity_test_env()
      ->identity_manager()
      ->GetAccountsCookieMutator()
      ->LogOutAllAccounts(gaia::GaiaSource::kChrome, base::DoNothing());
  EXPECT_EQ(diagnostics_provider()->GetDelayBeforeMakingCookieRequests(), zero);

  identity_test_env()->SimulateGaiaLogOutFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_GT(diagnostics_provider()->GetDelayBeforeMakingCookieRequests(), zero);
}
