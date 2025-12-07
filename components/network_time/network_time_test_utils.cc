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
// curl -i "http://clients2.google.com/time/1/current?cup2key=9:123123123"
//
// where 9 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this five times, so that the five
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1740704215210,\"server_nonce\":9."
    "745905051023761E19}",
    ")]}'\n{\"current_time_millis\":1740704303680,\"server_nonce\":5."
    "980509301132054E-279}",
    ")]}'\n{\"current_time_millis\":1740704348254,\"server_nonce\":-1."
    "9206278609497336E158}",
    ")]}'\n{\"current_time_millis\":1740704410539,\"server_nonce\":2."
    "8152807398526608E54}",
    ")]}'\n{\"current_time_millis\":1740704459047,\"server_nonce\":2."
    "934095446221426E135}"};

const char* kGoodTimeResponseServerProofHeader[] = {
    "3045022100a2bd5c42903ba33e71fab61df42c4d92100e7f3af1e5123ac127be7972349f10"
    "02207cc0cd0a3b96f9ee1bb1d7e405f35f8adabfa757c368c253fe4eee65baa39300:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "304502206642abea1998c7bcba589d0381da0a3b630c4d400b8bc1e066da22f21d9e628b02"
    "2100efef9b1be15f5c4de123c14daf0155fe4512156491c871c0170e1da7df53c769:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "304402201ef61d4f677e3837b661e79c6a9e153ee3e46444600565a2aeff603e60ab2b6102"
    "2077c6c22b5324575bdafe956d282ab2b5ac6d6ed2fb4204277530726fa3404c4c:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3046022100e263fe9c2541fb98ebcf5f7998f0957b4f3994d565cf0e97b9eaf33fb14ad6a8"
    "022100fa7d688f4fc929d31ab79a155f0b119739d0a7e5ab84da69495cdc47dbcb4a93:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30460221008fa418b8a9f934ff1ae305f9e956faf0e73c8c1cab05437e43d3d5bd2aa22a3b"
    "022100f98811ca4b3663e94b6ce1ee7b1214e00ff9944e8bd37121f36b78c57d0f7261:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
};

const double kGoodTimeResponseHandlerJsTime[] = {
    1740704215210, 1740704303680, 1740704348254, 1740704410539, 1740704459047};

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
