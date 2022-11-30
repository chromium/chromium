// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_test_utils.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

// Update as follows:
//
// curl -i http://clients2.google.com/time/1/current?cup2key=6:123123123
//
// where 6 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this five times, so that the five
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1652339069759,\"server_nonce\":7."
    "29375327039265E-230}",
    ")]}'\n{\"current_time_millis\":1652339136683,\"server_nonce\":1."
    "4794255040588188E-23}",
    ")]}'\n{\"current_time_millis\":1652339231311,\"server_nonce\":-4."
    "419622990529329E127}",
    ")]}'\n{\"current_time_millis\":1652339325263,\"server_nonce\":6."
    "315542071193776E16}",
    ")]}'\n{\"current_time_millis\":1652339380058,\"server_nonce\":-3."
    "8130598030275436E-131}"};

const char* kGoodTimeResponseServerProofHeader[] = {
    "3046022100ab673cb907cd0c9139da0d50ada4c3326929d455e46f8f797f0a8c511ef"
    "6881b02210091b0f77f463578b7c0be36d42f053de34e486eba8c0526f9f115f80c80"
    "7a5ce4:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30440220139b1710412e68cf445d39234158943efee3e2b27859b97582b478af7dcf6"
    "e85022004d9d7c432aae15a5207a18e25ae345675348767f784b7d3b07920b64a2ead"
    "c3:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3044022017d2ae7bf4507b18badd735629f1c44f1f024c88aeb271e4d52e6a849cb22"
    "7a3022052c1223d65b4488ccb47f2c882f249c91541a55b99752f4f487a3e6abc5194"
    "10:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30450221009b8db5fe3000e6e0b696baf8d42d40d7b4ff9757c84b49cdd6d85fa39cd"
    "0fca2022005144ed3eeb95707e3bc9e7369d8bd475b5d2f50ac98e5c56160bc9b1f1f"
    "d36a:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3046022100ec690467b5eb550e6b91ec65810d942ed859d3dd6f966f72c9489679825"
    "81cf8022100b2a54d11217ba6a75576e6db02f5293a70fd4bc27b02f0bda46e60f98a"
    "b05785:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

const double kGoodTimeResponseHandlerJsTime[] = {
    1652339069759, 1652339136683, 1652339231311, 1652339325263, 1652339380058};

std::unique_ptr<net::test_server::HttpResponse> GoodTimeResponseHandler(
    const net::test_server::HttpRequest& request) {
  net::test_server::BasicHttpResponse* response =
      new net::test_server::BasicHttpResponse();
  response->set_code(net::HTTP_OK);
  response->set_content(kGoodTimeResponseBody[0]);
  response->AddCustomHeader("x-cup-server-proof",
                            kGoodTimeResponseServerProofHeader[0]);
  return std::unique_ptr<net::test_server::HttpResponse>(response);
}

FieldTrialTest::FieldTrialTest() {}

FieldTrialTest::~FieldTrialTest() {}

void FieldTrialTest::SetFeatureParams(
    bool enable,
    float query_probability,
    NetworkTimeTracker::FetchBehavior fetch_behavior,
    NetworkTimeTracker::ClockDriftSamples clock_drift_samples) {
  scoped_feature_list_.Reset();
  if (!enable) {
    scoped_feature_list_.InitAndDisableFeature(kNetworkTimeServiceQuerying);
    return;
  }

  base::FieldTrialParams params;
  params["RandomQueryProbability"] = base::NumberToString(query_probability);
  // See string format defined by `base::TimeDeltaFromString`.
  params["CheckTimeInterval"] = "360s";
  params["ClockDriftSampleDistance"] = "2s";
  std::string fetch_behavior_param;
  switch (fetch_behavior) {
    case NetworkTimeTracker::FETCH_BEHAVIOR_UNKNOWN:
      NOTREACHED();
      fetch_behavior_param = "unknown";
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY:
      fetch_behavior_param = "background-only";
      break;
    case NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY:
      fetch_behavior_param = "on-demand-only";
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND:
      fetch_behavior_param = "background-and-on-demand";
      break;
  }
  params["FetchBehavior"] = fetch_behavior_param;

  std::string num_clock_drift_samples;
  switch (clock_drift_samples) {
    case NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES:
      num_clock_drift_samples = "0";
      break;
    case NetworkTimeTracker::ClockDriftSamples::TWO_SAMPLES:
      num_clock_drift_samples = "2";
      break;
    case NetworkTimeTracker::ClockDriftSamples::FOUR_SAMPLES:
      num_clock_drift_samples = "4";
      break;
    case NetworkTimeTracker::ClockDriftSamples::SIX_SAMPLES:
      num_clock_drift_samples = "6";
      break;
  }
  params["ClockDriftSamples"] = num_clock_drift_samples;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kNetworkTimeServiceQuerying, params);
}

}  // namespace network_time
