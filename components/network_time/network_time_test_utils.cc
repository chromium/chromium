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
// curl -i "http://clients2.google.com/time/1/current?cup2key=7:123123123"
//
// where 7 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this five times, so that the five
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1676407030473,\"server_nonce\":-7."
    "963518832647831E9}",
    ")]}'\n{\"current_time_millis\":1676407054804,\"server_nonce\":2."
    "660379914224199E64}",
    ")]}'\n{\"current_time_millis\":1676407071199,\"server_nonce\":5."
    "150419571081495E-301}",
    ")]}'\n{\"current_time_millis\":1676407085288,\"server_nonce\":-2."
    "3616379802443122E-151}",
    ")]}'\n{\"current_time_millis\":1676407147062,\"server_nonce\":2."
    "848761209101163E-158}"};

const char* kGoodTimeResponseServerProofHeader[] = {
    "304402200b875bcd9391c08c319504ea4cac3703b58f50797c99ed008068d232c62f0"
    "86802207babace8807a911ef7af424abdadac5ba118b06d1c68ddbe6c79e0f36da972"
    "18:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30450220600b8b15525d52aa939f8174a24b63d3eb2b7543cf1d3a36eabd572518ec5"
    "118022100d6e79c05701e981308d22e95f945a4a1b7cc2b2ce9c67b87fbec44e3d4b2"
    "c4a5:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022100f78429f21c9b5e4db283b7b4a81d2f91f51969aae4e9960edf0b82f21c3"
    "7c84a02200aa5e9e56bbf3cb16441355759425a31b0052fb371dfcb972d127cf185fe"
    "5424:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022015af65192d3f5024e068cee411b29c6c9093c159a6f5b7e7e8b9726a7eca3"
    "fa30221008dd1d69dc5e3cf364bb69366b3cc434a057fdf2c52475d92492eaca45f4b"
    "2ecf:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022100ca8b4a4af0bfacdcbb589ee8d0a7fb0e20e618bcf380c775e0aa573ef9d"
    "12f300220367ac76938af4b630d8c8ddef72b0c1e59630f505b110a1f3b18cd42229e"
    "4035:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

const double kGoodTimeResponseHandlerJsTime[] = {
    1676407030473, 1676407054804, 1676407071199, 1676407085288, 1676407147062};

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
  }
  params["ClockDriftSamples"] = num_clock_drift_samples;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kNetworkTimeServiceQuerying, params);
}

}  // namespace network_time
