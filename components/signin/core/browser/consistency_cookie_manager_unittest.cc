// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/consistency_cookie_manager_base.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {
namespace {

// GMock matcher that checks that the consistency cookie has the expected value.
MATCHER_P(CookieHasValueMatcher, value, "") {
  net::CookieOptions cookie_options;
  cookie_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX);
  return arg.Name() == "CHROME_ID_CONSISTENCY_STATE" && arg.Value() == value &&
         arg.IncludeForRequestURL(GaiaUrls::GetInstance()->gaia_url(),
                                  cookie_options)
             .IsInclude();
}

MATCHER(SetPermittedInContext, "") {
  const net::CanonicalCookie& cookie = testing::get<0>(arg);
  const net::CookieOptions& cookie_options = testing::get<1>(arg);
  return cookie.IsSetPermittedInContext(cookie_options).IsInclude();
}

class MockCookieManager
    : public testing::StrictMock<network::TestCookieManager> {
 public:
  // Adds a GMock expectation that the consistency cookie will be set with the
  // specified value.
  void ExpectSetCookieCall(const std::string& value) {
    EXPECT_CALL(*this, SetCanonicalCookie(CookieHasValueMatcher(value), "https",
                                          testing::_, testing::_))
        .With(testing::Args<0, 2>(SetPermittedInContext()));
  }

  MOCK_METHOD4(
      SetCanonicalCookie,
      void(const net::CanonicalCookie& cookie,
           const std::string& source_scheme,
           const net::CookieOptions& cookie_options,
           network::mojom::CookieManager::SetCanonicalCookieCallback callback));
};

class FakeConsistencyCookieManager : public ConsistencyCookieManagerBase {
 public:
  FakeConsistencyCookieManager(SigninClient* signin_client,
                               AccountReconcilor* reconcilor)
      : ConsistencyCookieManagerBase(signin_client, reconcilor) {
    UpdateCookie();
  }
};

class ConsistencyCookieManagerTest : public ::testing::Test {
 public:
  ConsistencyCookieManagerTest()
      : signin_client_(&pref_service_),
        identity_test_env_(/*test_url_loader_factory=*/nullptr,
                           &pref_service_,
                           AccountConsistencyMethod::kMirror,
                           &signin_client_) {
    scoped_feature_list_.InitAndEnableFeature(kMiceFeature);
    std::unique_ptr<MockCookieManager> cookie_manager =
        std::make_unique<MockCookieManager>();
    mock_cookie_manager_ = cookie_manager.get();
    signin_client_.set_cookie_manager(std::move(cookie_manager));
    reconcilor_ = std::make_unique<AccountReconcilor>(
        identity_test_env_.identity_manager(), &signin_client_,
        std::make_unique<AccountReconcilorDelegate>());
    reconcilor_->Initialize(/*start_reconcile_if_tokens_available=*/false);
  }

  ~ConsistencyCookieManagerTest() override { reconcilor_->Shutdown(); }

  SigninClient* signin_client() { return &signin_client_; }
  AccountReconcilor* reconcilor() { return reconcilor_.get(); }

  MockCookieManager* mock_cookie_manager() {
    DCHECK_EQ(mock_cookie_manager_, signin_client_.GetCookieManager());
    return mock_cookie_manager_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  // Owned by signin_client_.
  MockCookieManager* mock_cookie_manager_ = nullptr;

  TestSigninClient signin_client_;
  IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<AccountReconcilor> reconcilor_;
};

// Tests that the cookie is updated when the account reconcilor state changes.
TEST_F(ConsistencyCookieManagerTest, AccountReconcilorState) {
  // AccountReconcilor::Initialize() creates the ConsistencyCookieManager.
  mock_cookie_manager()->ExpectSetCookieCall("Consistent");
  reconcilor()->SetConsistencyCookieManager(
      std::make_unique<FakeConsistencyCookieManager>(signin_client(),
                                                     reconcilor()));
  testing::Mock::VerifyAndClearExpectations(mock_cookie_manager());
  ASSERT_EQ(signin_metrics::ACCOUNT_RECONCILOR_OK, reconcilor()->GetState());

  // Trigger a state change in the reconcilor, and check that the cookie was
  // updated accordingly. Calling EnableReconcile() will cause the reconcilor
  // state to change to SCHEDULED then OK.
  mock_cookie_manager()->ExpectSetCookieCall("Updating");
  mock_cookie_manager()->ExpectSetCookieCall("Consistent");
  reconcilor()->EnableReconcile();
}

}  // namespace
}  // namespace signin
