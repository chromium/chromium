// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_browser.h"

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

RequestHandlerForRegisterBrowser::RequestHandlerForRegisterBrowser(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForRegisterBrowser::~RequestHandlerForRegisterBrowser() = default;

std::string RequestHandlerForRegisterBrowser::RequestType() {
  return dm_protocol::kValueRequestTokenEnrollment;
}

std::unique_ptr<HttpResponse> RequestHandlerForRegisterBrowser::HandleRequest(
    const HttpRequest& request) {
  std::string enrollment_token;
  if (!GetEnrollmentTokenFromRequest(request, &enrollment_token)) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED,
                              "Missing enrollment token.");
  }

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::RegisterBrowserRequest& register_browser_request =
      device_management_request.register_browser_request();

  // Machine name is empty on mobile.
  if (register_browser_request.os_platform() != "Android" &&
      register_browser_request.os_platform() != "iOS" &&
      register_browser_request.machine_name().empty()) {
    LOG(ERROR) << "OS platform: " << register_browser_request.os_platform();
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Machine name must be non-empty on Desktop.");
  }

  if (enrollment_token == kInvalidEnrollmentToken) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED,
                              "Invalid enrollment token.");
  }

  std::string device_token = kFakeDeviceToken;
  em::DeviceManagementResponse device_management_response;
  device_management_response.mutable_register_response()
      ->set_device_management_token(device_token);

  ClientStorage::ClientInfo client_info;
  client_info.device_id =
      KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID);
  client_info.device_token = device_token;
  client_info.machine_name = register_browser_request.machine_name();
  client_info.allowed_policy_types.insert(
      {dm_protocol::kChromeMachineLevelUserCloudPolicyType,
       dm_protocol::kChromeMachineLevelUserCloudPolicyAndroidType,
       dm_protocol::kChromeMachineLevelUserCloudPolicyIOSType,
       dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
       dm_protocol::kChromeUserPolicyType});
  client_storage()->RegisterClient(std::move(client_info));

  return CreateHttpResponse(net::HTTP_OK,
                            device_management_response.SerializeAsString());
}

}  // namespace policy
