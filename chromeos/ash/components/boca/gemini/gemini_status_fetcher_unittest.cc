// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/gemini/gemini_status_fetcher.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

constexpr char kTestGaiaId[] = "12345";
constexpr char kFullUrl[] =
    "https://schooltools-pa.googleapis.com/v1/users/12345:getGeminiStatus";
constexpr char kGeminiEnabledPref[] = "ash.boca.gemini_enabled";

class GeminiStatusFetcherTest : public testing::Test {
 protected:
  void SetUp() override {
    GeminiStatusFetcher::RegisterProfilePrefs(pref_service_.registry());
    identity_test_env_.MakePrimaryAccountAvailable(
        "test_user@gmail.com", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(GeminiStatusFetcherTest, GetStatusReturnsSuccessResponse) {
  base::test::TestFuture<bool> future;
  auto fetcher = std::make_unique<GeminiStatusFetcher>(
      kTestGaiaId, identity_test_env_.identity_manager(),
      url_loader_factory_.GetSafeWeakWrapper(), &pref_service_);

  url_loader_factory_.AddResponse(
      kFullUrl, R"({"status": "ENABLEMENT_STATUS_DISABLED"})");
  fetcher->GetStatus(future.GetCallback());

  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(pref_service_.GetBoolean(kGeminiEnabledPref));
}

TEST_F(GeminiStatusFetcherTest, ConcurrentGetStatusQueuesCallbacks) {
  base::test::TestFuture<bool> future1;
  base::test::TestFuture<bool> future2;
  auto fetcher = std::make_unique<GeminiStatusFetcher>(
      kTestGaiaId, identity_test_env_.identity_manager(),
      url_loader_factory_.GetSafeWeakWrapper(), &pref_service_);

  url_loader_factory_.AddResponse(kFullUrl,
                                  R"({"status": "ENABLEMENT_STATUS_ENABLED"})");
  fetcher->GetStatus(future1.GetCallback());
  fetcher->GetStatus(future2.GetCallback());

  EXPECT_TRUE(future1.Get());
  EXPECT_TRUE(future2.Get());
  EXPECT_TRUE(pref_service_.GetBoolean(kGeminiEnabledPref));
}

class GeminiStatusFetcherCachedPrefTest
    : public GeminiStatusFetcherTest,
      public testing::WithParamInterface<bool> {};

TEST_P(GeminiStatusFetcherCachedPrefTest, GetStatusReturnsCachedPrefOnFailure) {
  bool cached_value = GetParam();
  pref_service_.SetBoolean(kGeminiEnabledPref, cached_value);
  base::test::TestFuture<bool> future;
  auto fetcher = std::make_unique<GeminiStatusFetcher>(
      kTestGaiaId, identity_test_env_.identity_manager(),
      url_loader_factory_.GetSafeWeakWrapper(), &pref_service_);

  url_loader_factory_.AddResponse(kFullUrl, "",
                                  net::HTTP_INTERNAL_SERVER_ERROR);
  fetcher->GetStatus(future.GetCallback());

  EXPECT_EQ(future.Get(), cached_value);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GeminiStatusFetcherCachedPrefTest,
                         testing::Bool());

}  // namespace
}  // namespace ash::boca
