// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_DEVICE_AND_USER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_DEVICE_AND_USER_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `register` (registration of devices and managed
// users).
class RequestHandlerForRegisterDeviceAndUser
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  RequestHandlerForRegisterDeviceAndUser(ClientStorage* client_storage,
                                         PolicyStorage* policy_storage);
  RequestHandlerForRegisterDeviceAndUser(
      RequestHandlerForRegisterDeviceAndUser&& handler) = delete;
  RequestHandlerForRegisterDeviceAndUser& operator=(
      RequestHandlerForRegisterDeviceAndUser&& handler) = delete;
  ~RequestHandlerForRegisterDeviceAndUser() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_DEVICE_AND_USER_H_
