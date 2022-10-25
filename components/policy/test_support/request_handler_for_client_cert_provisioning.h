// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CLIENT_CERT_PROVISIONING_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CLIENT_CERT_PROVISIONING_H_

#include <memory>
#include <string>

#include "components/policy/test_support/embedded_policy_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

// Handler for request type `client_cert_provisioning`.
class RequestHandlerForClientCertProvisioning
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForClientCertProvisioning(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForClientCertProvisioning(
      RequestHandlerForClientCertProvisioning&& handler) = delete;
  RequestHandlerForClientCertProvisioning& operator=(
      RequestHandlerForClientCertProvisioning&& handler) = delete;
  ~RequestHandlerForClientCertProvisioning() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_CLIENT_CERT_PROVISIONING_H_
