// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_api_authorization.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForApiAuthorization::RequestHandlerForApiAuthorization(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForApiAuthorization::~RequestHandlerForApiAuthorization() =
    default;

std::string RequestHandlerForApiAuthorization::RequestType() {
  return dm_protocol::kValueRequestApiAuthorization;
}

std::unique_ptr<HttpResponse> RequestHandlerForApiAuthorization::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementResponse device_management_response;
  device_management_response.mutable_service_api_access_response()
      ->set_auth_code(policy_storage()->robot_api_auth_code());
  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
