// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_TEST_TEST_SERVER_H_
#define CHROME_ENTERPRISE_COMPANION_TEST_TEST_SERVER_H_

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

class GURL;

namespace enterprise_companion {

// Defines a generic matcher to match expectations for a request.
using Matcher =
    base::RepeatingCallback<bool(const net::test_server::HttpRequest&)>;

// Defines a group of matchers which all must pass in order to match a request.
// This allows for combining several matchers when matching a single request.
using MatcherGroup = std::list<Matcher>;

class TestServer {
 public:
  TestServer();

  // Shuts down the server and verifies that all expectations were met and that
  // no extra communications were received.
  ~TestServer();

  // Registers an expected request with the server. Requests must match the
  // expectation defined by applying all individual request matchers composing
  // the `request_matcher_group` in the order the expectations were set.
  // The server replies with an HTTP 200 and `response_body` to an expected
  // request. It replies with HTTP 500 and fails the test if a request does
  // not match the next expected `request_matcher_group`, or if there are no
  // more expected requests. If the server does not receive every expected
  // request, it will fail the test during destruction.
  void ExpectOnce(const MatcherGroup& request_matcher_group,
                  const std::string& response_body,
                  net::HttpStatusCode response_status_code = net::HTTP_OK);

  bool HasUnmetExpectations() { return !request_matcher_groups_.empty(); }

  GURL crash_upload_url() const {
    return test_server_->GetURL("/crash-upload");
  }

  GURL device_management_encrypted_reporting_url() const {
    return test_server_->GetURL("/dm-encrypted-reporting");
  }

  GURL device_management_realtime_reporting_url() const {
    return test_server_->GetURL("/dm-realtime-reporting");
  }

  GURL event_logging_url() const {
    return test_server_->GetURL("/event-logging");
  }

  GURL proxy_pac_url() const { return test_server_->GetURL("/proxy.pac"); }

  GURL base_url() const { return test_server_->GetURL("/"); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_ =
      std::make_unique<net::test_server::EmbeddedTestServer>();
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::list<MatcherGroup> request_matcher_groups_;
  std::list<std::pair<net::HttpStatusCode, std::string>> responses_;
};

// Creates a matcher that expects an event log containing the provided events
// and outcomes.
Matcher CreateEventLogMatcher(
    const TestServer& test_server,
    const std::vector<std::pair<proto::EnterpriseCompanionEvent::EventCase,
                                EnterpriseCompanionStatus>>& expected_events);

// Creates a matcher that expects a request for a pac script.
Matcher CreatePacUrlMatcher(const TestServer& test_server);

// Create and serialize a LogResponse proto.
std::string CreateLogResponse(
    base::TimeDelta next_request_wait = base::Seconds(0));

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_TEST_TEST_SERVER_H_
