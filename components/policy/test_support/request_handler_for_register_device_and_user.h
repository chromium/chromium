// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_DEVICE_AND_USER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_DEVICE_AND_USER_H_

#include "components/policy/test_support/embedded_policy_test_server.h"
#include "device_management_backend.pb.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

// Handler for request type `register` (registration of devices and managed
// users).
class RequestHandlerForRegisterDeviceAndUser
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForRegisterDeviceAndUser(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForRegisterDeviceAndUser(
      RequestHandlerForRegisterDeviceAndUser&& handler) = delete;
  RequestHandlerForRegisterDeviceAndUser& operator=(
      RequestHandlerForRegisterDeviceAndUser&& handler) = delete;
  ~RequestHandlerForRegisterDeviceAndUser() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;

 protected:
  std::unique_ptr<net::test_server::HttpResponse> RegisterDeviceAndSendResponse(
      const net::test_server::HttpRequest& request,
      const enterprise_management::DeviceRegisterRequest& register_request,
      const std::string& policy_user);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_DEVICE_AND_USER_H_
