// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REALTIME_REPORTING_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REALTIME_REPORTING_H_

#include <memory>
#include <string>

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace net::test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

namespace policy {

inline constexpr char kRealtimeReportingRequestType[] = "realtime_reporting";

// Handler for /realtime_reporting requests.
class RequestHandlerForRealtimeReporting
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForRealtimeReporting(EmbeddedPolicyTestServer* parent);
  RequestHandlerForRealtimeReporting(
      RequestHandlerForRealtimeReporting&& handler) = delete;
  RequestHandlerForRealtimeReporting& operator=(
      RequestHandlerForRealtimeReporting&& handler) = delete;
  ~RequestHandlerForRealtimeReporting() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REALTIME_REPORTING_H_
