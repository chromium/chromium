// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_DEVICE_ATTRIBUTE_UPDATE_PERMISSION_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_DEVICE_ATTRIBUTE_UPDATE_PERMISSION_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `device_attribute_update_permission`.
class RequestHandlerForDeviceAttributeUpdatePermission
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForDeviceAttributeUpdatePermission(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForDeviceAttributeUpdatePermission(
      RequestHandlerForDeviceAttributeUpdatePermission&& handler) = delete;
  RequestHandlerForDeviceAttributeUpdatePermission& operator=(
      RequestHandlerForDeviceAttributeUpdatePermission&& handler) = delete;
  ~RequestHandlerForDeviceAttributeUpdatePermission() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_DEVICE_ATTRIBUTE_UPDATE_PERMISSION_H_
