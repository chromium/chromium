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
// curl -i "http://clients2.google.com/time/1/current?cup2key=10:123123123"
//
// where 10 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this five times, so that the five
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1772475870963,\"server_nonce\":-1."
    "1540523495618042E81}",
    ")]}'\n{\"current_time_millis\":1772475955240,\"server_nonce\":-3."
    "040201861059435E185}",
    ")]}'\n{\"current_time_millis\":1772476006702,\"server_nonce\":1."
    "9754674126500175E95}",
    ")]}'\n{\"current_time_millis\":1772476051135,\"server_nonce\":-1."
    "1437939165251215E-199}",
    ")]}'\n{\"current_time_millis\":1772476093870,\"server_nonce\":7."
    "949563237287921E-250}"};

const char* kGoodTimeResponseServerProofHeader[] = {
    "304402201c1eaf3acb3cfdbbc8a26582b29a2e72d384b605a86d75e6bc7d195d823ea7e002"
    "205c678f7eb08f8cafcf1da14a5f169531df764e8a454b3ed54fb88b243a0e90a3:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30460221009bc7670d1d93f149b768bd6ea6ac08fc6eb7f9414b51c7bf36f16f696dc4e22a"
    "0221008cb56923075d0efa3f0769d1e211560bb67a5f19c4a228885aebb36a3a13b7cb:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022100e30cf0863744f6891225884a5b62d4813e44f6efac09dd975c62dc7cd68d62a5"
    "0220163890ce0d98b78558caeb9177bb859bb97299ef23a3d40d74972b003c2387fd:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30450220210acd166a9337c3774c76a0c2e36d0366622c031ddcbc9beb8691a417f13dde02"
    "2100dfdc586699b8dcf77a459cade999b1e36fb619777358626e44119790ee8d552b:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3045022056e434fa3afe2759d23a4e9116809bf84938752f9640f47845ea70b5393f6ba902"
    "2100f9fd992712c027a7703f5e700423e1e5b793cd27807350a68848adf04d3705a5:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
};

const double kGoodTimeResponseHandlerJsTime[] = {
    1772475870963, 1772475955240, 1772476006702, 1772476051135, 1772476093870};

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

FieldTrialTest::FieldTrialTest() = default;

FieldTrialTest::~FieldTrialTest() = default;

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
