// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager.h"

#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/test_signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace signin {

namespace {

class MockCookieManager
    : public testing::StrictMock<network::TestCookieManager> {
 public:
  MOCK_METHOD4(SetCanonicalCookie,
               void(const net::CanonicalCookie& cookie,
                    const GURL& source_url,
                    const net::CookieOptions& cookie_options,
                    SetCanonicalCookieCallback callback));
};

}  // namespace

class ConsistencyCookieManagerTest : public testing::Test {
 public:
  ConsistencyCookieManagerTest() {
    std::unique_ptr<MockCookieManager> mock_cookie_manager =
        std::make_unique<MockCookieManager>();
    cookie_manager_ = mock_cookie_manager.get();
    signin_client_.set_cookie_manager(std::move(mock_cookie_manager));
  }

  ~ConsistencyCookieManagerTest() override { reconcilor_.Shutdown(); }

  void ExpectCookieSet(const std::string& value) {
    const std::string expected_domain =
        std::string(".") + GaiaUrls::GetInstance()->gaia_url().host();
    EXPECT_CALL(
        *cookie_manager_,
        SetCanonicalCookie(
            testing::AllOf(
                testing::Property(&net::CanonicalCookie::Name,
                                  ConsistencyCookieManager::kCookieName),
                testing::Property(&net::CanonicalCookie::Value, value),
                testing::Property(&net::CanonicalCookie::Domain,
                                  expected_domain),
                testing::Property(&net::CanonicalCookie::Path, "/"),
                testing::Property(&net::CanonicalCookie::IsSecure, true),
                testing::Property(&net::CanonicalCookie::IsHttpOnly, false)),
            GaiaUrls::GetInstance()->gaia_url(), testing::_, testing::_));
  }

  void SetReconcilorState(signin_metrics::AccountReconcilorState state) {
    account_reconcilor()->SetState(state);
  }

  AccountReconcilor* account_reconcilor() { return &reconcilor_; }
  SigninClient* signin_client() { return &signin_client_; }
  MockCookieManager* cookie_manager() { return cookie_manager_; }

 private:
  TestSigninClient signin_client_{/*prefs=*/nullptr};
  MockCookieManager* cookie_manager_ = nullptr;  // Owned by `signin_client_`.
  AccountReconcilor reconcilor_{/*identity_manager=*/nullptr, &signin_client_,
                                std::make_unique<AccountReconcilorDelegate>()};
};

TEST_F(ConsistencyCookieManagerTest, ReconcilorState) {
  // Initial state.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_INACTIVE);
  ConsistencyCookieManager cookie_consistency_manager(signin_client(),
                                                      account_reconcilor());
  // Cookie has not been set.
  testing::Mock::VerifyAndClearExpectations(cookie_manager());

  struct AccountReconcilorStateTestCase {
    signin_metrics::AccountReconcilorState state;
    absl::optional<std::string> cookie_value;
  };

  // Iterate over all reconcilor state and check that they map to the right
  // cookie value.
  // Notes about the order:
  // - Don't start with OK, as this is the current state.
  // - Always change the reconcilor state to something that results in a
  //   different cookie value (otherwise the cookie is not updated).
  AccountReconcilorStateTestCase cases[] = {
      {signin_metrics::ACCOUNT_RECONCILOR_RUNNING,
       ConsistencyCookieManager::kCookieValueStringUpdating},
      {signin_metrics::ACCOUNT_RECONCILOR_OK,
       ConsistencyCookieManager::kCookieValueStringConsistent},
      {signin_metrics::ACCOUNT_RECONCILOR_ERROR,
       ConsistencyCookieManager::kCookieValueStringInconsistent},
      {signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED,
       ConsistencyCookieManager::kCookieValueStringUpdating},
      {signin_metrics::ACCOUNT_RECONCILOR_INACTIVE, absl::nullopt},
  };

  for (const AccountReconcilorStateTestCase& test_case : cases) {
    if (test_case.cookie_value.has_value())
      ExpectCookieSet(test_case.cookie_value.value());
    SetReconcilorState(test_case.state);
    testing::Mock::VerifyAndClearExpectations(cookie_manager());
  }

  // Check that the cookie is not updated needlessly.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_INACTIVE);
  // Set again the state that was used before INACTIVE.
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Setting the same state again does not update the cookie.
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
  // Setting a state that maps to the same value does not update the cookie.
  EXPECT_EQ(account_reconcilor()->GetState(),
            signin_metrics::ACCOUNT_RECONCILOR_SCHEDULED);
  SetReconcilorState(signin_metrics::ACCOUNT_RECONCILOR_RUNNING);
  testing::Mock::VerifyAndClearExpectations(cookie_manager());
}

}  // namespace signin
