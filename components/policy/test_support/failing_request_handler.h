// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_FAILING_REQUEST_HANDLER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_FAILING_REQUEST_HANDLER_H_

#include "components/policy/test_support/embedded_policy_test_server.h"
#include "net/http/http_status_code.h"

namespace policy {

// Handler that always returns specified error code for a given request type.
class FailingRequestHandler : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  FailingRequestHandler(ClientStorage* client_storage,
                        PolicyStorage* policy_storage,
                        const std::string& request_type,
                        net::HttpStatusCode error_code);
  FailingRequestHandler(FailingRequestHandler&& handler) = delete;
  FailingRequestHandler& operator=(FailingRequestHandler&& handler) = delete;
  ~FailingRequestHandler() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  std::string request_type_;
  net::HttpStatusCode error_code_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_FAILING_REQUEST_HANDLER_H_
