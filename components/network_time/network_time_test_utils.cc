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
// curl -i "http://clients2.google.com/time/1/current?cup2key=8:123123123"
//
// where 8 is the key version and 123123123 is the nonce.  Copy the response
// and the x-cup-server-proof header into |kGoodTimeResponseBody| and
// |kGoodTimeResponseServerProofHeader| respectively, and the
// 'current_time_millis' value of the response into
// |kGoodTimeResponseHandlerJsTime|.  Do this five times, so that the five
// requests appear in order below.
const char* kGoodTimeResponseBody[] = {
    ")]}'\n{\"current_time_millis\":1707850276713,\"server_nonce\":3."
    "12767027640506E-119}",
    ")]}'\n{\"current_time_millis\":1707850276847,\"server_nonce\":-5."
    "835329309579494E-23}",
    ")]}'\n{\"current_time_millis\":1707850276975,\"server_nonce\":4."
    "012053606543568E-64}",
    ")]}'\n{\"current_time_millis\":1707850277110,\"server_nonce\":-4."
    "979081124791505E145}",
    ")]}'\n{\"current_time_millis\":1707850277238,\"server_nonce\":7."
    "324863778238033E240}"};

const char* kGoodTimeResponseServerProofHeader[] = {
    "3045022054b5032071e0e1b8254c0aaf3b5d1c241508e60690ca3a431b70ac1cabbd6dbd02"
    "2100f49ab1dac93b7787d2a918e30f06c948fef9d46811140b52f978bb963e171d85:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3044022056349d9bf7037653721d510b8d034f10e108c18b52894787adbf701ce7f399c302"
    "202ee2a43ccc91d9ed69b2ba830e2a391446030848ede1a7b40654feac9a579b1e:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3044022014ceed219cab9d0e9a429c7c6c8f2c21f7a1f335ff8f666352b128fa0d847ce802"
    "200a59aec15ecb0450ad8abfe96dc331b7aa2e136c4e193484b8fd8b575146080a:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "3044022070c5f7bb0b6536940e11f063b6b9e4822435f08bf96cf90f89e95b27f537940202"
    "200a49bf1c5f16b802a47982f819621026d3f28e986b7f65866f20eef89e8499f6:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "30460221009ee20fd89cc2933d95c9069db4fb95d0dde12b6308cd0f6902f88b34b1a80a20"
    "022100bae4573e1177be340523038b65b6f0658b270b9698f1db353379d95821b3ea66:"
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
};

const double kGoodTimeResponseHandlerJsTime[] = {
    1707850276713, 1707850276847, 1707850276975, 1707850277110, 1707850277238};

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
      NOTREACHED_IN_MIGRATION();
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
