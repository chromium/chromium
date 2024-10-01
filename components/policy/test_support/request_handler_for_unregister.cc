// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_unregister.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForUnregister::RequestHandlerForUnregister(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForUnregister::~RequestHandlerForUnregister() = default;

std::string RequestHandlerForUnregister::RequestType() {
  return dm_protocol::kValueRequestUnregister;
}

std::unique_ptr<HttpResponse> RequestHandlerForUnregister::HandleRequest(
    const HttpRequest& request) {
  std::string request_device_token;
  if (!GetDeviceTokenFromRequest(request, &request_device_token) ||
      !client_storage()->DeleteClient(request_device_token)) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "Invalid device token.");
  }

  em::DeviceManagementResponse device_management_response;
  device_management_response.mutable_unregister_response();
  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
