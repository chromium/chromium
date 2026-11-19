// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHROME_REPORT_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHROME_REPORT_H_

#include <string>

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace net::test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

namespace policy {

// Handler for `chrome_desktop_report` and `chrome_profile_report` requests.
class RequestHandlerForChromeReport
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  RequestHandlerForChromeReport(EmbeddedPolicyTestServer* parent,
                                std::string request_type);
  RequestHandlerForChromeReport(RequestHandlerForChromeReport&& handler) =
      delete;
  RequestHandlerForChromeReport& operator=(
      RequestHandlerForChromeReport&& handler) = delete;
  ~RequestHandlerForChromeReport() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  const std::string request_type_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CHROME_REPORT_H_
