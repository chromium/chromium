// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_info_fetcher.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy {

namespace {

static const char kUserInfoUrl[] =
    "https://www.googleapis.com/oauth2/v1/userinfo";
static const char kUserInfoResponse[] =
    "{"
    "  \"email\": \"test_user@test.com\","
    "  \"verified_email\": true,"
    "  \"hd\": \"test.com\""
    "}";

class MockUserInfoFetcherDelegate : public UserInfoFetcher::Delegate {
 public:
  MockUserInfoFetcherDelegate() = default;
  ~MockUserInfoFetcherDelegate() = default;
  MOCK_METHOD1(OnGetUserInfoFailure,
               void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnGetUserInfoSuccess, void(const base::Value::Dict& result));
};

MATCHER_P(MatchDict, expected, "matches Value::Dict") {
  return arg == *expected;
}

class UserInfoFetcherTest : public testing::Test {
 public:
  UserInfoFetcherTest() = default;
  UserInfoFetcherTest(const UserInfoFetcherTest&) = delete;
  UserInfoFetcherTest& operator=(const UserInfoFetcherTest&) = delete;
  ~UserInfoFetcherTest() override = default;

 protected:
  base::test::TaskEnvironment task_env_;
  network::TestURLLoaderFactory loader_factory_;
};

TEST_F(UserInfoFetcherTest, FailedFetch) {
  base::HistogramTester histogram_tester;

  MockUserInfoFetcherDelegate delegate;
  UserInfoFetcher fetcher(
      &delegate,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &loader_factory_));
  fetcher.Start("access_token");

  // Fake a failed fetch - should result in the failure callback being invoked.
  EXPECT_CALL(delegate, OnGetUserInfoFailure(_));
  EXPECT_TRUE(loader_factory_.SimulateResponseForPendingRequest(
      kUserInfoUrl, std::string(), net::HTTP_INTERNAL_SERVER_ERROR));
  task_env_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Enterprise.UserInfoFetch.Status",
      EnterpriseUserInfoFetchStatus::kFailedWithNetworkError, 1);
  histogram_tester.ExpectUniqueSample("Enterprise.UserInfoFetch.HttpErrorCode",
                                      500, 1);
}

TEST_F(UserInfoFetcherTest, SuccessfulFetch) {
  MockUserInfoFetcherDelegate delegate;
  UserInfoFetcher fetcher(
      &delegate,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &loader_factory_));
  fetcher.Start("access_token");

  // Generate what we expect our result will look like (should match
  // parsed kUserInfoResponse).
  base::Value::Dict dict;
  dict.Set("email", "test_user@test.com");
  dict.Set("verified_email", true);
  dict.Set("hd", "test.com");

  // Fake a successful fetch - should result in the data being parsed and
  // the values passed off to the success callback.
  EXPECT_CALL(delegate, OnGetUserInfoSuccess(MatchDict(&dict)));
  EXPECT_TRUE(loader_factory_.SimulateResponseForPendingRequest(
      kUserInfoUrl, kUserInfoResponse));
}

TEST_F(UserInfoFetcherTest, FetchResponseNotParsableToJSON) {
  base::HistogramTester histogram_tester;

  MockUserInfoFetcherDelegate delegate;
  UserInfoFetcher fetcher(
      &delegate,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &loader_factory_));
  fetcher.Start("access_token");

  // Fake a successful fetch - should result in the data being parsed and
  // the values passed off to the success callback.
  EXPECT_CALL(delegate, OnGetUserInfoFailure(GoogleServiceAuthError(
                            GoogleServiceAuthError::CONNECTION_FAILED)));
  EXPECT_TRUE(loader_factory_.SimulateResponseForPendingRequest(
      kUserInfoUrl, "<content>not json</content>"));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.UserInfoFetch.Status",
      EnterpriseUserInfoFetchStatus::kCantParseJsonInResponse, 1);
}

TEST_F(UserInfoFetcherTest, FetchResponseNotDict) {
  base::HistogramTester histogram_tester;

  MockUserInfoFetcherDelegate delegate;
  UserInfoFetcher fetcher(
      &delegate,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &loader_factory_));
  fetcher.Start("access_token");

  // Fake a successful fetch - should result in the data being parsed and
  // the values passed off to the success callback.
  EXPECT_CALL(delegate, OnGetUserInfoFailure(GoogleServiceAuthError(
                            GoogleServiceAuthError::CONNECTION_FAILED)));
  EXPECT_TRUE(loader_factory_.SimulateResponseForPendingRequest(kUserInfoUrl,
                                                                "[1, 2, 3]"));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.UserInfoFetch.Status",
      EnterpriseUserInfoFetchStatus::kResponseIsNotDict, 1);
}

}  // namespace

}  // namespace policy
