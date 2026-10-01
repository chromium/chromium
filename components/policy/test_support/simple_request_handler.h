// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_SIMPLE_REQUEST_HANDLER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_SIMPLE_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Returns HTTP 200 with the same DeviceManagementResponse for every request
// of one type. Use it only when the response does not depend on the request
// or on the server state.
class SimpleRequestHandler : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  SimpleRequestHandler(
      EmbeddedPolicyTestServer* parent,
      std::string request_type,
      enterprise_management::DeviceManagementResponse response);
  ~SimpleRequestHandler() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 private:
  const std::string request_type_;
  const enterprise_management::DeviceManagementResponse response_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_SIMPLE_REQUEST_HANDLER_H_
