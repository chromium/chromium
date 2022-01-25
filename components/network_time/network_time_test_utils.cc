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
// curl -i http://clients2.google.com/time/1/current?cup2key=5:123123123
//
// where 5 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this three times, so that the three
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1619464140565,"
    "\"server_nonce\":-1.656679479914492E230}",
    ")]}'\n{\"current_time_millis\":1619464273366,"
    "\"server_nonce\":2.1195306862817135E-5}",
    ")]}'\n{\"current_time_millis\":1642162812422,"
    "\"server_nonce\":3.374791108303444E207}"};
const char* kGoodTimeResponseServerProofHeader[] = {
    "3045022100f829ced2af34ade53400f66eef6df9af732fa8bfe08517287c2805c92891e321"
    "022062fb405b2cf12bc3e2ac037985c4b8065a62e86e29a2e745ebff80fd52189c6a:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3046022100c78436ad47904634aacd33f4c4bcb55bd6f7f2ed84a620fda0deaede99c32de6"
    "022100b595458bd03d83f33bfb891de1327b26620d576937f3713af59bb1f2c53f2e8b:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022100f50a8e6f97d8c362f878e2c988ab2c983e536de4dc2116a314699a4010d7d8b8"
    "02202ace752a45721399ec6dd704da55700c9b11626f7c55a72037db255b79992476:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
const double kGoodTimeResponseHandlerJsTime[] = {1619464140565, 1619464273366,
                                                 1642162812422};

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
