// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_test_utils.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_time {

// Update as follows:
//
// curl -i http://clients2.google.com/time/1/current?cup2key=2:123123123
//
// where 2 is the key version and 123123123 is the nonce.  Copy the
// response and the x-cup-server-proof header into
// |kGoodTimeResponseBody| and |kGoodTimeResponseServerProofHeader|
// respectively, and the 'current_time_millis' value of the response
// into |kGoodTimeResponseHandlerJsTime|.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1522081016324,"
    "\"server_nonce\":-1.475187036492045E154}",
    ")]}'\n{\"current_time_millis\":1522096305984,"
    "\"server_nonce\":-1.1926302260014708E-276}"};
const char* kGoodTimeResponseServerProofHeader[] = {
    "3046022100c0351a20558bac037253f3969547f82805b340f51de06461e83f33b41f8e85d3"
    "022100d04162c448438e5462df4bf6171ef26c53ec7d3a0cb915409e8bec6c99c69c67:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "304402201758cc66f7be58692362dad351ee71ecce78bd8491c8bfe903da39ea048ff67d02"
    "203aa51acfac9462b19ef3e6d6c885a60cb0858a274ae97506934737d8e66bc081:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
const double kGoodTimeResponseHandlerJsTime[] = {1522081016324, 1522096305984};

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

void FieldTrialTest::SetNetworkQueriesWithVariationsService(
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
  params["CheckTimeIntervalSeconds"] = base::NumberToString(360);
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
