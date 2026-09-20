// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_identity_delegation_helper.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/test/test_cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace lens {

namespace {

class FakeCookieManager : public network::TestCookieManager {
 public:
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override {
    cookies_.push_back(cookie);
    if (callback) {
      std::move(callback).Run(net::CookieAccessResult());
    }
  }

  void GetCookieList(
      const GURL& url,
      const net::CookieOptions& cookie_options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override {
    net::CookieAccessResultList result;
    for (const auto& cookie : cookies_) {
      result.push_back({cookie, net::CookieAccessResult()});
    }
    std::move(callback).Run(result, {});
  }

 private:
  std::vector<net::CanonicalCookie> cookies_;
};

GenerateSapisidHashCallback GetFakeGenerator() {
  return base::BindRepeating(
      [](const std::string& email, const std::string& sapisid_cookie,
         const std::string& origin,
         base::Time timestamp) -> std::optional<std::string> {
        return "SAPISIDHASH 12345_fakehash";
      });
}

}  // namespace

class LensIdentityDelegationHelperTest : public testing::Test {
 protected:
  void SetSapisidCookie(const std::string& value) {
    auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
        "SAPISID", value, ".google.com", "/", base::Time(), base::Time(),
        base::Time(), base::Time(), /*secure=*/true, /*httponly=*/false,
        net::CookieSameSite::NO_RESTRICTION,
        net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
        net::CookieSourceType::kHTTP);

    cookie_manager_.SetCanonicalCookie(*cookie, GURL("https://google.com"),
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
  }

  base::test::TaskEnvironment task_environment_;
  FakeCookieManager cookie_manager_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SignedOut) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future.GetCallback());

  // Signed out: should only return Origin header.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kSignedOut, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.SignedOut", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchCookies", 0);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_NullCookieManagerOrIdentityManager) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<std::vector<std::string>> future1;
  FetchIdentityDelegationHeaders(nullptr, identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future1.GetCallback());
  EXPECT_THAT(future1.Get(), ElementsAre("Origin", "https://www.google.com"));

  base::test::TestFuture<std::vector<std::string>> future2;
  FetchIdentityDelegationHeaders(&cookie_manager_, nullptr,
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future2.GetCallback());
  EXPECT_THAT(future2.Get(), ElementsAre("Origin", "https://www.google.com"));

  histogram_tester.ExpectBucketCount(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kNoCookieManager, 1);
  histogram_tester.ExpectBucketCount(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kSignedOut, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 2);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.NoCookieManager", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.SignedOut", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_OriginCanonicalization) {
  base::test::TestFuture<std::vector<std::string>> future;
  // Pass an origin with trailing slash and path.
  FetchIdentityDelegationHeaders(
      &cookie_manager_, identity_test_env_.identity_manager(),
      "https://www.google.com/search?q=test/", GetFakeGenerator(), std::nullopt,
      future.GetCallback());

  // Origin should be normalized and canonicalized without trailing slash or
  // path.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SignedIn_NoCookie) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  // Force update cookie jar accounts in IdentityManager.
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future.GetCallback());

  // Signed in but no cookie: should only return Origin header.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kNoSapisidCookie, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.NoSapisidCookie", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchCookies", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SignedIn_WithCookie) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  ASSERT_EQ(headers.size(), 6u);
  EXPECT_EQ(headers[0], "Origin");
  EXPECT_EQ(headers[1], "https://www.google.com");
  EXPECT_EQ(headers[2], "Authorization");
  EXPECT_EQ(headers[3], "SAPISIDHASH 12345_fakehash");
  EXPECT_EQ(headers[4], "X-Goog-AuthUser");
  EXPECT_EQ(headers[5], "0");  // Index 0 in cookie jar

  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.Success", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchCookies", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_MultipleAccounts_PrimaryMatches) {
  // Primary is user2
  signin::SimpleAccountAvailabilityOptions options;
  options.primary_account_consent_level = signin::ConsentLevel::kSignin;
  options.gaia_id = GaiaId("gaia_id_2");

  identity_test_env_.MakeAccountAvailable("user2@gmail.com", options);
  // Cookie jar has user1 (index 0) and user2 (index 1)
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1")},
       {"user2@gmail.com", GaiaId("gaia_id_2")}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  ASSERT_EQ(headers.size(), 6u);
  EXPECT_EQ(headers[4], "X-Goog-AuthUser");
  EXPECT_EQ(headers[5], "1");  // user2 is at index 1
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_MultipleAccounts_NoPrimaryMatch) {
  base::HistogramTester histogram_tester;
  // No primary account (web-only sign-in)
  // Cookie jar has user1 (index 0) and user2 (index 1)
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1")},
       {"user2@gmail.com", GaiaId("gaia_id_2")}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future.GetCallback());

  // Should fall back to signed-out behavior (Origin header only).
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kAccountError, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.AccountError", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_PrimaryAccount_PersistentError) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});
  SetSapisidCookie("sapisid_value");

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 std::nullopt, future.GetCallback());

  // Persistent error on primary account: should fall back to signed-out
  // behavior.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kAccountError, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.AccountError", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SpecificIndex_PersistentError) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});
  SetSapisidCookie("sapisid_value");

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 /*authuser_index=*/0, future.GetCallback());

  // Persistent error on candidate account: should fall back to signed-out
  // behavior.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kAccountError, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.AccountError", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SpecificIndex_SignedOutCookieAccount) {
  base::HistogramTester histogram_tester;
  // Entire cookie jar only has signed-out accounts (zero valid signed-in
  // accounts).
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1"), /*signed_out=*/true}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 /*authuser_index=*/0, future.GetCallback());

  // Signed out account in cookie jar: should fall back to signed-out behavior.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kSignedOut, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.SignedOut", 1);
}

TEST_F(
    LensIdentityDelegationHelperTest,
    FetchIdentityDelegationHeaders_MultipleAccounts_SpecificIndex_SignedOut) {
  base::HistogramTester histogram_tester;
  // Cookie jar has user1 (signed-in at index 0) and user2 (signed-out at index
  // 1).
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1"), /*signed_out=*/false},
       {"user2@gmail.com", GaiaId("gaia_id_2"), /*signed_out=*/true}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 /*authuser_index=*/1, future.GetCallback());

  // Requesting index 1 which is signed out: should fall back to signed-out
  // behavior with kAccountError.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kAccountError, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.AccountError", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_MultipleAccounts_SpecificIndex) {
  base::HistogramTester histogram_tester;
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1")},
       {"user2@gmail.com", GaiaId("gaia_id_2")}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", GetFakeGenerator(),
                                 /*authuser_index=*/1, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  ASSERT_EQ(headers.size(), 6u);
  EXPECT_EQ(headers[4], "X-Goog-AuthUser");
  EXPECT_EQ(headers[5], "1");

  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.Success", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchCookies", 1);
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_HashFailed) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});
  SetSapisidCookie("sapisid_value");

  GenerateSapisidHashCallback failing_generator = base::BindRepeating(
      [](const std::string&, const std::string&, const std::string&,
         base::Time) -> std::optional<std::string> { return std::nullopt; });

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(&cookie_manager_,
                                 identity_test_env_.identity_manager(),
                                 "https://www.google.com", failing_generator,
                                 std::nullopt, future.GetCallback());

  // Failed hash generation: returns origin header and records kHashFailed.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
  histogram_tester.ExpectUniqueSample(
      "Lens.IdentityDelegation.FetchHeadersStatus",
      LensIdentityDelegationFetchStatus::kHashFailed, 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchHeaders.HashFailed", 1);
  histogram_tester.ExpectTotalCount(
      "Lens.IdentityDelegation.TimeToFetchCookies", 1);
}

}  // namespace lens
