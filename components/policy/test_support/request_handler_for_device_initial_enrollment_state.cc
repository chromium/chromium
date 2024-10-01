// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_device_initial_enrollment_state.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForDeviceInitialEnrollmentState::
    RequestHandlerForDeviceInitialEnrollmentState(
        EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForDeviceInitialEnrollmentState::
    ~RequestHandlerForDeviceInitialEnrollmentState() = default;

std::string RequestHandlerForDeviceInitialEnrollmentState::RequestType() {
  return dm_protocol::kValueRequestInitialEnrollmentStateRetrieval;
}

std::unique_ptr<HttpResponse>
RequestHandlerForDeviceInitialEnrollmentState::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::DeviceInitialEnrollmentStateRequest& state_request =
      device_management_request.device_initial_enrollment_state_request();

  const PolicyStorage::InitialEnrollmentState* state =
      policy_storage()->GetInitialEnrollmentState(
          state_request.brand_code() + "_" + state_request.serial_number());
  em::DeviceManagementResponse device_management_response;
  em::DeviceInitialEnrollmentStateResponse* state_response =
      device_management_response
          .mutable_device_initial_enrollment_state_response();
  state_response->set_initial_enrollment_mode(state->initial_enrollment_mode);
  state_response->set_management_domain(state->management_domain);
  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
