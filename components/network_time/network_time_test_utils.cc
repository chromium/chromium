// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_test_utils.h"

#include <memory>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_time/network_time_tracker.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"

namespace network_time {

std::unique_ptr<net::test_server::HttpResponse> GoodTimeResponseHandler(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(kGoodTimeResponseBody[0]);
  response->AddCustomHeader("x-cup-server-proof",
                            kGoodTimeResponseServerProofHeader[0]);
  return response;
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
