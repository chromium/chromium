// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/test/test_server.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/proto/enterprise_companion_event.pb.h"
#include "chrome/enterprise_companion/proto/log_request.pb.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_companion {

using HttpRequest = net::test_server::HttpRequest;
using HttpResponse = net::test_server::HttpResponse;
using QueryParams = base::flat_map<std::string, std::string>;

TestServer::TestServer() {
  test_server_->RegisterRequestHandler(
      base::BindRepeating(&TestServer::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));
}

TestServer::~TestServer() {
  for (const auto& request_matcher_group : request_matcher_groups_) {
    // Forces `request_matcher` to log to help debugging, unless the
    // matcher matches the empty request.
    ADD_FAILURE() << "Unmet expectation: ";
    base::ranges::for_each(request_matcher_group,
                           [](Matcher matcher) { matcher.Run(HttpRequest()); });
  }
}

void TestServer::ExpectOnce(const MatcherGroup& request_matcher_group,
                            const std::string& response_body,
                            net::HttpStatusCode response_status_code) {
  request_matcher_groups_.push_back(std::move(request_matcher_group));
  responses_.emplace_back(response_status_code, response_body);
}

std::unique_ptr<HttpResponse> TestServer::HandleRequest(
    const HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request_matcher_groups_.empty()) {
    VLOG(0) << "Unexpected request.";
    ADD_FAILURE() << "Unexpected request with URL: " << request.GetURL();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  if (!base::ranges::all_of(
          request_matcher_groups_.front(),
          [&request](Matcher matcher) { return matcher.Run(request); })) {
    VLOG(0) << "Request did not match.";
    ADD_FAILURE() << "Unmatched request to " << request.GetURL();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  const auto& [response_code, response_body] = responses_.front();
  response->set_code(response_code);
  response->set_content(response_body);
  request_matcher_groups_.pop_front();
  responses_.pop_front();
  return response;
}

Matcher CreateEventLogMatcher(
    const TestServer& test_server,
    const std::vector<std::pair<proto::EnterpriseCompanionEvent::EventCase,
                                EnterpriseCompanionStatus>>& expected_events) {
  return base::BindRepeating(
      [](const GURL& url,
         const std::vector<std::pair<proto::EnterpriseCompanionEvent::EventCase,
                                     EnterpriseCompanionStatus>>&
             expected_events,
         const HttpRequest& request) {
        if (request.GetURL() != url ||
            request.method != net::test_server::METHOD_POST ||
            !request.has_content) {
          return false;
        }

        proto::LogRequest log_request;
        if (!log_request.ParseFromString(request.content)) {
          return false;
        }

        // The following values should match for all event pings.
        EXPECT_EQ(log_request.client_info().client_type(),
                  proto::ClientInfo_ClientType_CHROME_ENTERPRISE_COMPANION);
        EXPECT_EQ(log_request.log_source(),
                  proto::CHROME_ENTERPRISE_COMPANION_APP);
        if (log_request.log_event().size() != 1) {
          ADD_FAILURE() << "Malformed event log proto, wrong number of events.";
          return false;
        }

        proto::ChromeEnterpriseCompanionAppExtension extension;
        if (!extension.ParseFromString(
                log_request.log_event().at(0).source_extension())) {
          ADD_FAILURE() << "Malformed event log proto, cannot parse extension.";
          return false;
        }

        return base::ranges::equal(
            extension.event(), expected_events, /*pred=*/{},
            [](const proto::EnterpriseCompanionEvent& event) {
              return std::make_pair(
                  event.event_case(),
                  EnterpriseCompanionStatus::FromProtoStatus(event.status()));
            });
      },
      test_server.event_logging_url(), std::move(expected_events));
}

Matcher CreatePacUrlMatcher(const TestServer& test_server) {
  return base::BindRepeating([](const net::test_server::HttpRequest& request) {
    return request.relative_url == "/proxy.pac";
  });
}

std::string CreateLogResponse(base::TimeDelta next_request_wait) {
  proto::LogResponse response;
  response.set_next_request_wait_millis(next_request_wait.InMilliseconds());
  return response.SerializeAsString();
}

}  // namespace enterprise_companion
