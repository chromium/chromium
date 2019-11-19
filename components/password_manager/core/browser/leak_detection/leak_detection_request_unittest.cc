// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_api.pb.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using ::testing::Eq;

class LeakDetectionRequestTest : public testing::Test {
 public:
  static constexpr char kAccessToken[] = "access_token";
  static constexpr char kUsername[] = "username";
  static constexpr char kPassword[] = "password123";

  ~LeakDetectionRequestTest() override = default;

  base::test::TaskEnvironment& task_env() { return task_env_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  LeakDetectionRequest& request() { return request_; }

 private:
  base::test::TaskEnvironment task_env_;
  base::HistogramTester histogram_tester_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  LeakDetectionRequest request_;
};

// Note: These strings are static member constants rather than namespace scoped
// constants to avoid compilation errors in Jumbo builds.
constexpr char LeakDetectionRequestTest::kAccessToken[];
constexpr char LeakDetectionRequestTest::kUsername[];
constexpr char LeakDetectionRequestTest::kPassword[];

TEST_F(LeakDetectionRequestTest, ServerError) {
  test_url_loader_factory()->AddResponse(
      LeakDetectionRequest::kLookupSingleLeakEndpoint, "",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::MockCallback<LeakDetectionRequest::LookupSingleLeakCallback> callback;
  request().LookupSingleLeak(test_url_loader_factory(), kAccessToken, kUsername,
                             kPassword, callback.Get());
  EXPECT_CALL(callback, Run(Eq(nullptr)));
  task_env().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.LookupSingleLeakResponseResult",
      LeakDetectionRequest::LeakLookupResponseResult::kFetchError, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.HttpResponseCode",
      net::HTTP_INTERNAL_SERVER_ERROR, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.NetErrorCode",
      -net::ERR_HTTP_RESPONSE_CODE_FAILURE, 1);
}

TEST_F(LeakDetectionRequestTest, MalformedServerResponse) {
  static constexpr base::StringPiece kMalformedResponse = "\x01\x02\x03";
  test_url_loader_factory()->AddResponse(
      LeakDetectionRequest::kLookupSingleLeakEndpoint,
      std::string(kMalformedResponse));

  base::MockCallback<LeakDetectionRequest::LookupSingleLeakCallback> callback;
  request().LookupSingleLeak(test_url_loader_factory(), kAccessToken, kUsername,
                             kPassword, callback.Get());
  EXPECT_CALL(callback, Run(Eq(nullptr)));
  task_env().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.LookupSingleLeakResponseResult",
      LeakDetectionRequest::LeakLookupResponseResult::kParseError, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.SingleLeakResponseSize",
      kMalformedResponse.size(), 1);
}

TEST_F(LeakDetectionRequestTest, WellformedServerResponse) {
  google::internal::identity::passwords::leak::check::v1::
      LookupSingleLeakResponse response;
  std::string response_string = response.SerializeAsString();
  test_url_loader_factory()->AddResponse(
      LeakDetectionRequest::kLookupSingleLeakEndpoint, response_string);

  base::MockCallback<LeakDetectionRequest::LookupSingleLeakCallback> callback;
  request().LookupSingleLeak(test_url_loader_factory(), kAccessToken, kUsername,
                             kPassword, callback.Get());
  EXPECT_CALL(callback, Run(testing::Pointee(SingleLookupResponse())));
  task_env().RunUntilIdle();

  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.LookupSingleLeakResponseResult",
      LeakDetectionRequest::LeakLookupResponseResult::kSuccess, 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.SingleLeakResponseSize",
      response_string.size(), 1);
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.LeakDetection.SingleLeakResponsePrefixes",
      response.encrypted_leak_match_prefix().size(), 1);
}

}  // namespace password_manager
