// Copyright 2016 The Chromium Authors. All rights reserved.
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
// |kGoodTimeResponseHandlerJsTime|.  Do this three times, so that the three
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1650390717683,"
    "\"server_nonce\":-8.681945473178043E-158}",
    ")]}'\n{\"current_time_millis\":1650390778336,"
    "\"server_nonce\":-5.552005431494712E-267}",
    ")]}'\n{\"current_time_millis\":1650400732787,"
    "\"server_nonce\":1.1469515554432679E-54}"};
const char* kGoodTimeResponseServerProofHeader[] = {
    "3045022100986ea0776d59a62ebbe0e35caec58bf37190941bf6cdf303eaba8a2bcfbc2431"
    "02201a3a6860394ae531bf64d5f91b8d52c8f1b657b485931e307c1dc50ba5a7c109:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022100c8d22dbb620c0570f3ea1a73f40b843bbc7fa69c792e576f186bfe9a0f85eb88"
    "02204655e9a9f6260a9216966b2296180b814bf3098734fbd07c8aa7b4e28f9b8dc0:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30450221008825845f57896040d494ffd1d683f4ea3fdff1b84fb377109d23ae80eaba59f4"
    "02201ab77f038ba448034e5e5d567f78ba137440d0c7a27b41d413b473bb3222d5d4:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
const double kGoodTimeResponseHandlerJsTime[] = {1650390717683, 1650390778336,
                                                 1650400732787};

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
    NetworkTimeTracker::FetchBehavior fetch_behavior) {
  scoped_feature_list_.Reset();
  if (!enable) {
    scoped_feature_list_.InitAndDisableFeature(kNetworkTimeServiceQuerying);
    return;
  }

  base::FieldTrialParams params;
  params["RandomQueryProbability"] = base::NumberToString(query_probability);
  // See string format defined by `base::TimeDeltaFromString`.
  params["CheckTimeInterval"] = "360s";
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
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kNetworkTimeServiceQuerying, params);
}

}  // namespace network_time
