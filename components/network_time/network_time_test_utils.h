// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/network_time/network_time_tracker.h"

namespace net::test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

namespace network_time {

inline constexpr const char* kGoodTimeResponseBody[] = {
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

// The x-cup-server-proof header values that should be served along with
// |kGoodTimeResponseBody| to make a test server response be accepted by
// NetworkTimeTracker as a valid response.
inline constexpr const char* kGoodTimeResponseServerProofHeader[] = {
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

// The times that |kGoodTimeResponseBody| uses. Can be converted to a
// base::Time with base::Time::FromMillisecondsSinceUnixEpoch.
inline constexpr double kGoodTimeResponseHandlerJsTime[] = {
    1772475870963, 1772475955240, 1772476006702, 1772476051135, 1772476093870};

// Returns a valid network time response using the constants above. See
// comments in the .cc for how to update the time returned in the response.
std::unique_ptr<net::test_server::HttpResponse> GoodTimeResponseHandler(
    const net::test_server::HttpRequest& request);

// Allows unit tests to configure the network time queries field trial.
class FieldTrialTest {
 public:
  FieldTrialTest();

  FieldTrialTest(const FieldTrialTest&) = delete;
  FieldTrialTest& operator=(const FieldTrialTest&) = delete;

  virtual ~FieldTrialTest();

  void SetFeatureParams(bool enable,
                        float query_probability,
                        NetworkTimeTracker::FetchBehavior fetch_behavior);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TEST_UTILS_H_
