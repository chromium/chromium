// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_device_attribute_update_permission.h"

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

RequestHandlerForDeviceAttributeUpdatePermission::
    RequestHandlerForDeviceAttributeUpdatePermission(
        EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForDeviceAttributeUpdatePermission::
    ~RequestHandlerForDeviceAttributeUpdatePermission() = default;

std::string RequestHandlerForDeviceAttributeUpdatePermission::RequestType() {
  return dm_protocol::kValueRequestDeviceAttributeUpdatePermission;
}

std::unique_ptr<HttpResponse>
RequestHandlerForDeviceAttributeUpdatePermission::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementResponse device_management_response;
  em::DeviceAttributeUpdatePermissionResponse::ResultType result =
      policy_storage()->allow_set_device_attributes()
          ? em::DeviceAttributeUpdatePermissionResponse::
                ATTRIBUTE_UPDATE_ALLOWED
          : em::DeviceAttributeUpdatePermissionResponse::
                ATTRIBUTE_UPDATE_DISALLOWED;
  device_management_response
      .mutable_device_attribute_update_permission_response()
      ->set_result(result);
  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
