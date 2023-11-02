// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_DEVICE_INITIAL_ENROLLMENT_STATE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_DEVICE_INITIAL_ENROLLMENT_STATE_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `device_state_retrieval`.
class RequestHandlerForDeviceInitialEnrollmentState
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  explicit RequestHandlerForDeviceInitialEnrollmentState(
      EmbeddedPolicyTestServer* parent);
  RequestHandlerForDeviceInitialEnrollmentState(
      RequestHandlerForDeviceInitialEnrollmentState&& handler) = delete;
  RequestHandlerForDeviceInitialEnrollmentState& operator=(
      RequestHandlerForDeviceInitialEnrollmentState&& handler) = delete;
  ~RequestHandlerForDeviceInitialEnrollmentState() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_DEVICE_INITIAL_ENROLLMENT_STATE_H_
