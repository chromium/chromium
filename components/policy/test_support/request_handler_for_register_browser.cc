// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_browser.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
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

RequestHandlerForRegisterBrowserOrPolicyAgent::
    RequestHandlerForRegisterBrowserOrPolicyAgent(
        EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

std::unique_ptr<HttpResponse>
RequestHandlerForRegisterBrowserOrPolicyAgent::HandleRequest(
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

  if (std::unique_ptr<HttpResponse> validation_response =
          ValidateRegisterBrowserRequest(register_browser_request)) {
    return validation_response;
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
  base::ranges::copy(allowed_policy_types(),
                     std::inserter(client_info.allowed_policy_types,
                                   client_info.allowed_policy_types.end()));
  client_storage()->RegisterClient(std::move(client_info));

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

RequestHandlerForRegisterBrowserOrPolicyAgent::
    ~RequestHandlerForRegisterBrowserOrPolicyAgent() = default;

RequestHandlerForRegisterBrowser::RequestHandlerForRegisterBrowser(
    EmbeddedPolicyTestServer* parent)
    : RequestHandlerForRegisterBrowserOrPolicyAgent(parent) {}

RequestHandlerForRegisterBrowser::~RequestHandlerForRegisterBrowser() = default;

std::string RequestHandlerForRegisterBrowser::RequestType() {
  return dm_protocol::kValueRequestRegisterBrowser;
}

std::unique_ptr<HttpResponse>
RequestHandlerForRegisterBrowser::ValidateRegisterBrowserRequest(
    const enterprise_management::RegisterBrowserRequest&
        register_browser_request) {
  // Machine name is empty on mobile.
  if (register_browser_request.os_platform() != "Android" &&
      register_browser_request.os_platform() != "iOS" &&
      register_browser_request.machine_name().empty()) {
    VLOG(1) << "OS platform: " << register_browser_request.os_platform();
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Machine name must be non-empty on Desktop.");
  }
  return nullptr;
}

constexpr base::flat_set<std::string>
RequestHandlerForRegisterBrowser::allowed_policy_types() {
  return base::MakeFlatSet<std::string>(
      std::vector({dm_protocol::kChromeMachineLevelUserCloudPolicyType,
                   dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
                   dm_protocol::kChromeUserPolicyType}));
}

RequestHandlerForRegisterPolicyAgent::RequestHandlerForRegisterPolicyAgent(
    EmbeddedPolicyTestServer* parent)
    : RequestHandlerForRegisterBrowserOrPolicyAgent(parent) {}

RequestHandlerForRegisterPolicyAgent::~RequestHandlerForRegisterPolicyAgent() =
    default;

std::string RequestHandlerForRegisterPolicyAgent::RequestType() {
  return dm_protocol::kValueRequestRegisterPolicyAgent;
}

std::unique_ptr<HttpResponse>
RequestHandlerForRegisterPolicyAgent::ValidateRegisterBrowserRequest(
    const em::RegisterBrowserRequest& register_browser_request) {
  // Policy agents are only defined for Google Updater-supported platforms.
  if (register_browser_request.os_platform() != "Linux" &&
      register_browser_request.os_platform() != "Mac OS X" &&
      register_browser_request.os_platform() != "Windows") {
    LOG(ERROR) << "Invalid OS platform: "
               << register_browser_request.os_platform();
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Invalid platform.");
  }

  if (register_browser_request.machine_name().empty()) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Machine name must be non-empty.");
  }

  return nullptr;
}

constexpr base::flat_set<std::string>
RequestHandlerForRegisterPolicyAgent::allowed_policy_types() {
  return base::MakeFlatSet<std::string>(
      std::vector({dm_protocol::kGoogleUpdateMachineLevelAppsPolicyType,
                   dm_protocol::kGoogleUpdateMachineLevelOmahaPolicyType,
                   dm_protocol::kChromeMachineLevelUserCloudPolicyType,
                   dm_protocol::kChromeMachineLevelExtensionCloudPolicyType}));
}

}  // namespace policy
