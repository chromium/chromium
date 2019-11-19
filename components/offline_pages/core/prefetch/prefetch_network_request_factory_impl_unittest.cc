// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"

#include "base/bind_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Contains;
using testing::Not;

namespace offline_pages {

// TODO(dimich): Add tests that cancel/fail/complete the requests and
// verify that the tests are removed form maps etc.
class PrefetchNetworkRequestFactoryTest : public PrefetchRequestTestBase {
 public:
  PrefetchNetworkRequestFactoryTest();

  PrefetchNetworkRequestFactoryImpl* request_factory() {
    return request_factory_.get();
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  std::unique_ptr<PrefetchNetworkRequestFactoryImpl> request_factory_;
  TestingPrefServiceSimple pref_service_;
};

PrefetchNetworkRequestFactoryTest::PrefetchNetworkRequestFactoryTest() {
  prefetch_prefs::RegisterPrefs(pref_service()->registry());
  request_factory_ = std::make_unique<PrefetchNetworkRequestFactoryImpl>(
      shared_url_loader_factory(), version_info::Channel::UNKNOWN,
      "a user agent", pref_service());
}

TEST_F(PrefetchNetworkRequestFactoryTest, TestMakeGetOperationRequest) {
  // Query whether there is an operation to start with, before we make a
  // request.
  std::string operation_name = "an operation";
  EXPECT_FALSE(request_factory()->HasOutstandingRequests());
  GetOperationRequest* request =
      request_factory()->FindGetOperationRequestByName(operation_name);
  EXPECT_EQ(nullptr, request);

  auto operation_names = request_factory()->GetAllOperationNamesRequested();
  EXPECT_TRUE(operation_names->empty());

  // Then, make the request and ensure we can find it by name.
  request_factory()->MakeGetOperationRequest(operation_name, base::DoNothing());
  EXPECT_TRUE(request_factory()->HasOutstandingRequests());
  request = request_factory()->FindGetOperationRequestByName(operation_name);
  EXPECT_NE(nullptr, request);

  operation_names = request_factory()->GetAllOperationNamesRequested();
  EXPECT_EQ(1UL, operation_names->size());
  EXPECT_THAT(*operation_names, Contains(operation_name));

  // Then check that a request is not found for another name (which was not
  // requested).
  std::string operation_name_2 = "another operation";
  EXPECT_TRUE(request_factory()->HasOutstandingRequests());
  GetOperationRequest* request_2 =
      request_factory()->FindGetOperationRequestByName(operation_name_2);
  EXPECT_EQ(nullptr, request_2);

  // Then make the second request.
  request_factory()->MakeGetOperationRequest(operation_name_2,
                                             base::DoNothing());

  // Query for the second request, ensure it is different than the first
  // request, and ensure it didn't change the first request.
  request_2 =
      request_factory()->FindGetOperationRequestByName(operation_name_2);
  EXPECT_NE(nullptr, request_2);
  EXPECT_EQ(request,
            request_factory()->FindGetOperationRequestByName(operation_name));
  EXPECT_NE(request, request_2);

  // Then overwrite the first request with a new one, and make sure it's
  // different.
  request_factory()->MakeGetOperationRequest(operation_name, base::DoNothing());
  EXPECT_NE(request,
            request_factory()->FindGetOperationRequestByName(operation_name));
}

TEST_F(PrefetchNetworkRequestFactoryTest, TestMakeGeneratePageBundleRequest) {
  std::vector<std::string> urls = {"example.com/1", "example.com/2"};
  std::string reg_id = "a registration id";

  EXPECT_FALSE(request_factory()->HasOutstandingRequests());

  request_factory()->MakeGeneratePageBundleRequest(urls, reg_id,
                                                   base::DoNothing());

  EXPECT_TRUE(request_factory()->HasOutstandingRequests());

  auto requested_urls = request_factory()->GetAllUrlsRequested();
  EXPECT_THAT(*requested_urls, Contains(urls[0]));
  EXPECT_THAT(*requested_urls, Contains(urls[1]));

  std::vector<std::string> urls2 = {"example.com/3"};
  request_factory()->MakeGeneratePageBundleRequest(urls2, reg_id,
                                                   base::DoNothing());
  requested_urls = request_factory()->GetAllUrlsRequested();
  EXPECT_THAT(*requested_urls, Contains(urls[0]));
  EXPECT_THAT(*requested_urls, Contains(urls[1]));
  EXPECT_THAT(*requested_urls, Contains(urls2[0]));
}

TEST_F(PrefetchNetworkRequestFactoryTest, ManyGenerateBundleRequests) {
  std::vector<std::string> urls1 = {"example.com/1"};
  std::string reg_id = "a registration id";
  const int kTooManyRequests = 20;

  for (int i = 0; i < kTooManyRequests; ++i) {
    request_factory()->MakeGeneratePageBundleRequest(urls1, reg_id,
                                                     base::DoNothing());
  }

  // Add one more request, over the maximum count of concurrent requests.
  std::vector<std::string> urls2 = {"example.com/2"};
  request_factory()->MakeGeneratePageBundleRequest(urls2, reg_id,
                                                   base::DoNothing());

  auto requested_urls = request_factory()->GetAllUrlsRequested();
  EXPECT_THAT(*requested_urls, Contains(urls1[0]));
  // Requests over maximum concurrent count of requests should not be made.
  EXPECT_THAT(*requested_urls, Not(Contains(urls2[0])));
}

TEST_F(PrefetchNetworkRequestFactoryTest, ManyGetOperationRequests) {
  std::string operation_name1 = "an operation 1";
  const int kTooManyRequests = 20;

  for (int i = 0; i < kTooManyRequests; ++i) {
    request_factory()->MakeGetOperationRequest(operation_name1,
                                               base::DoNothing());
  }

  // Add one more request, over the maximum count of concurrent requests.
  std::string operation_name2 = "an operation 2";
  request_factory()->MakeGetOperationRequest(operation_name2,
                                             base::DoNothing());

  auto operation_names = request_factory()->GetAllOperationNamesRequested();
  EXPECT_THAT(*operation_names, Contains(operation_name1));
  // Requests over maximum concurrent count of requests should not be made.
  EXPECT_THAT(*operation_names, Not(Contains(operation_name2)));

  EXPECT_NE(nullptr,
            request_factory()->FindGetOperationRequestByName(operation_name1));
  // Requests over maximum concurrent count of requests should not be made.
  EXPECT_EQ(nullptr,
            request_factory()->FindGetOperationRequestByName(operation_name2));
}

TEST_F(PrefetchNetworkRequestFactoryTest, ManyRequestsMixedType) {
  std::string operation_name1 = "an operation 1";
  const int kNotTooManyRequests = 6;

  for (int i = 0; i < kNotTooManyRequests; ++i) {
    request_factory()->MakeGetOperationRequest(operation_name1,
                                               base::DoNothing());
  }

  // Still possible to make more requests...
  std::string operation_name2 = "an operation 2";
  request_factory()->MakeGetOperationRequest(operation_name2,
                                             base::DoNothing());

  auto operation_names = request_factory()->GetAllOperationNamesRequested();
  EXPECT_THAT(*operation_names, Contains(operation_name1));
  EXPECT_THAT(*operation_names, Contains(operation_name2));

  // This should get the factory over the max number of allowed requests.
  std::vector<std::string> urls1 = {"example.com/1"};
  std::string reg_id = "a registration id";
  for (int i = 0; i < kNotTooManyRequests; ++i) {
    request_factory()->MakeGeneratePageBundleRequest(urls1, reg_id,
                                                     base::DoNothing());
  }

  // Add one more request, over the maximum count of concurrent requests.
  std::string operation_name3 = "an operation 3";
  request_factory()->MakeGetOperationRequest(operation_name3,
                                             base::DoNothing());

  operation_names = request_factory()->GetAllOperationNamesRequested();
  EXPECT_THAT(*operation_names, Contains(operation_name1));
  EXPECT_THAT(*operation_names, Contains(operation_name2));
  // Requests over maximum concurrent count of requests should not be made.
  EXPECT_THAT(*operation_names, Not(Contains(operation_name3)));
}

TEST_F(PrefetchNetworkRequestFactoryTest, GetOperationRequestDoneUMA) {
  base::HistogramTester histogram_tester;
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  EXPECT_CALL(callback, Run(testing::_, testing::_, testing::_));

  // Make a request.
  std::string operation_name1 = "an operation 1";
  request_factory()->MakeGetOperationRequest(operation_name1, callback.Get());

  // Have the test system callback into GetOperationRequestDone with an error
  // code that will cause the SHOULD_SUSPEND response.
  RespondWithHttpError(net::HTTP_NOT_IMPLEMENTED);
  RunUntilIdle();

  // Check that operation names have been cleaned up.
  EXPECT_FALSE(request_factory()->HasOutstandingRequests());
  EXPECT_TRUE(request_factory()->GetAllOperationNamesRequested()->empty());
  EXPECT_EQ(nullptr,
            request_factory()->FindGetOperationRequestByName(operation_name1));

  // TODO(petewil): We should also test that when the maximum concurrent
  // requests have been reached, operations resumes to normal once they are
  // handled.

  // Ensure that the status was recorded in UMA.
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.ServiceGetOperationStatus",
      static_cast<int>(PrefetchRequestStatus::kShouldSuspendNotImplemented), 1);
}

TEST_F(PrefetchNetworkRequestFactoryTest, GeneratePageBundleRequestDoneUMA) {
  base::HistogramTester histogram_tester;
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  EXPECT_CALL(callback, Run(testing::_, testing::_, testing::_));

  // Make a request.
  std::vector<std::string> urls1 = {"example.com/1"};
  std::string reg_id = "a registration id";
  request_factory()->MakeGeneratePageBundleRequest(urls1, reg_id,
                                                   callback.Get());

  // Have the test framework call back into the GeneratePageBundleRequestDone
  // method with an error code that will produce kShouldRetryWithoutBackoff.
  RespondWithNetError(net::ERR_NETWORK_CHANGED);
  RunUntilIdle();

  // Make sure the operation data got cleaned up properly.
  EXPECT_FALSE(request_factory()->HasOutstandingRequests());
  EXPECT_TRUE(request_factory()->GetAllUrlsRequested()->empty());

  // Ensure that the status was recorded in UMA.
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.ServiceGetPageBundleStatus",
      static_cast<int>(PrefetchRequestStatus::kShouldRetryWithoutBackoff), 1);
}

}  // namespace offline_pages
