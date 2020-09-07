// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/password_manager/core/browser/change_password_url_service_impl.h"

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kMockResponse[] = R"({
  "google.com": "https://google.com/change-password",
  "a.blogspot.com": "https://a.blogspot.com/change-password",
  "web.app": "https://web.app/change-password"
})";
}  // namespace

namespace password_manager {

class ChangePasswordUrlServiceTest : public testing::Test {
 public:
  ChangePasswordUrlServiceTest() {
    SetMockResponse();
    // Password Manager is enabled by default.
    test_pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
  }

  // Fetches the url overrides and waits until the response arrived.
  void PrefetchAndWaitUntilDone();

  void PrefetchURLs() { change_password_url_service_.PrefetchURLs(); }

  void DisablePasswordManagerEnabledPolicy() {
    test_pref_service_.SetBoolean(
        password_manager::prefs::kCredentialsEnableService, false);
  }

  GURL GetChangePasswordUrl(const GURL& url) {
    return change_password_url_service_.GetChangePasswordUrl(url);
  }
  void SetMockResponse(const std::string& response = kMockResponse) {
    test_url_loader_factory_.AddResponse(
        password_manager::ChangePasswordUrlServiceImpl::
            kChangePasswordUrlOverrideUrl,
        response);
  }

  void ClearMockResponses() { test_url_loader_factory_.ClearResponses(); }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  TestingPrefServiceSimple test_pref_service_;
  ChangePasswordUrlServiceImpl change_password_url_service_{
      test_shared_loader_factory_, &test_pref_service_};
  base::HistogramTester histogram_tester_;
};

void ChangePasswordUrlServiceTest::PrefetchAndWaitUntilDone() {
  change_password_url_service_.PrefetchURLs();
  task_environment_.RunUntilIdle();
}

TEST_F(ChangePasswordUrlServiceTest, eTLDLookup) {
  // TODO(crbug.com/1086141): If possible mock eTLD registry to ensure sites are
  // listed.
  PrefetchAndWaitUntilDone();

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")),
            GURL("https://google.com/change-password"));
  EXPECT_EQ(GetChangePasswordUrl(GURL("https://a.google.com/foo")),
            GURL("https://google.com/change-password"));

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://web.app")), GURL());

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://blogspot.com")), GURL());
  EXPECT_EQ(GetChangePasswordUrl(GURL("https://a.blogspot.com")),
            GURL("https://a.blogspot.com/change-password"));
  EXPECT_EQ(GetChangePasswordUrl(GURL("https://b.blogspot.com")), GURL());

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://notlisted.com/foo")), GURL());
}

TEST_F(ChangePasswordUrlServiceTest, PassworManagerPolicyDisabled) {
  DisablePasswordManagerEnabledPolicy();
  PrefetchAndWaitUntilDone();

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")), GURL());
}

TEST_F(ChangePasswordUrlServiceTest, NetworkRequestFails_RetryWorks) {
  ClearMockResponses();
  PrefetchURLs();

  // Waiting for response.
  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")), GURL());

  test_url_loader_factory().SimulateResponseForPendingRequest(
      GURL(password_manager::ChangePasswordUrlServiceImpl::
               kChangePasswordUrlOverrideUrl),
      network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_NOT_FOUND), "");

  // 404 response received.
  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")), GURL());

  SetMockResponse();
  PrefetchAndWaitUntilDone();

  // Successful response arrived.
  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")),
            GURL("https://google.com/change-password"));
}

TEST_F(ChangePasswordUrlServiceTest,
       GetChangePasswordUrlMetrics_NotFetchedYet) {
  ClearMockResponses();
  PrefetchURLs();

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")), GURL());
  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordUrlMetricName,
      metrics_util::GetChangePasswordUrlMetric::kNotFetchedYet, 1);
}

TEST_F(ChangePasswordUrlServiceTest,
       GetChangePasswordUrlMetrics_UrlOverrideUsed) {
  PrefetchAndWaitUntilDone();

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://google.com/foo")),
            GURL("https://google.com/change-password"));
  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordUrlMetricName,
      metrics_util::GetChangePasswordUrlMetric::kUrlOverrideUsed, 1);
}

TEST_F(ChangePasswordUrlServiceTest,
       GetChangePasswordUrlMetrics_NoUrlOverrideAvailable) {
  PrefetchAndWaitUntilDone();

  EXPECT_EQ(GetChangePasswordUrl(GURL("https://netflix.com")), GURL());
  histogram_tester().ExpectUniqueSample(
      kGetChangePasswordUrlMetricName,
      metrics_util::GetChangePasswordUrlMetric::kNoUrlOverrideAvailable, 1);
}

TEST_F(ChangePasswordUrlServiceTest, NetworkMetrics_Failed) {
  ClearMockResponses();
  PrefetchAndWaitUntilDone();

  // Still waiting for response
  histogram_tester().ExpectTotalCount(kGstaticFetchTimeMetricName, 0);
  histogram_tester().ExpectTotalCount(kGstaticFetchHttpResponseCodeMetricName,
                                      0);
  histogram_tester().ExpectTotalCount(kGstaticFetchErrorCodeMetricName, 0);
  histogram_tester().ExpectTotalCount(
      kChangePasswordUrlServiceFetchResultMetricName, 0);

  // Set response
  test_url_loader_factory().SimulateResponseForPendingRequest(
      GURL(password_manager::ChangePasswordUrlServiceImpl::
               kChangePasswordUrlOverrideUrl),
      network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_NOT_FOUND), "");

  histogram_tester().ExpectTotalCount(kGstaticFetchTimeMetricName, 1);
  histogram_tester().ExpectUniqueSample(kGstaticFetchHttpResponseCodeMetricName,
                                        net::HTTP_NOT_FOUND, 1);
  histogram_tester().ExpectUniqueSample(kGstaticFetchErrorCodeMetricName,
                                        -net::ERR_HTTP_RESPONSE_CODE_FAILURE,
                                        1);
  histogram_tester().ExpectUniqueSample(
      kChangePasswordUrlServiceFetchResultMetricName,
      ChangePasswordUrlServiceFetchResult::kFailure, 1);
}

TEST_F(ChangePasswordUrlServiceTest, NetworkMetrics_Success) {
  PrefetchAndWaitUntilDone();

  histogram_tester().ExpectTotalCount(kGstaticFetchTimeMetricName, 1);
  histogram_tester().ExpectTotalCount(kGstaticFetchHttpResponseCodeMetricName,
                                      0);
  histogram_tester().ExpectTotalCount(kGstaticFetchErrorCodeMetricName, 0);
  histogram_tester().ExpectUniqueSample(
      kChangePasswordUrlServiceFetchResultMetricName,
      ChangePasswordUrlServiceFetchResult::kSuccess, 1);
}

TEST_F(ChangePasswordUrlServiceTest, NetworkMetrics_Malformed) {
  SetMockResponse("invelid_json");
  PrefetchAndWaitUntilDone();

  histogram_tester().ExpectTotalCount(kGstaticFetchTimeMetricName, 1);
  histogram_tester().ExpectTotalCount(kGstaticFetchHttpResponseCodeMetricName,
                                      0);
  histogram_tester().ExpectTotalCount(kGstaticFetchErrorCodeMetricName, 0);
  histogram_tester().ExpectUniqueSample(
      kChangePasswordUrlServiceFetchResultMetricName,
      ChangePasswordUrlServiceFetchResult::kMalformed, 1);
}

}  // namespace password_manager
