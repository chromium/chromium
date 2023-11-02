// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_CERT_BASED_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_CERT_BASED_H_

#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/request_handler_for_register_device_and_user.h"

namespace policy {

// Handler for request type `certificate_based_register`.
class RequestHandlerForRegisterCertBased
    : public RequestHandlerForRegisterDeviceAndUser {
 public:
  explicit RequestHandlerForRegisterCertBased(EmbeddedPolicyTestServer* parent);
  RequestHandlerForRegisterCertBased(
      RequestHandlerForRegisterCertBased&& handler) = delete;
  RequestHandlerForRegisterCertBased& operator=(
      RequestHandlerForRegisterCertBased&& handler) = delete;
  ~RequestHandlerForRegisterCertBased() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_CERT_BASED_H_
