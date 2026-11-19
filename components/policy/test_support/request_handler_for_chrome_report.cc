// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_chrome_report.h"

#include <string>
#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/report_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

RequestHandlerForChromeReport::RequestHandlerForChromeReport(
    EmbeddedPolicyTestServer* parent,
    std::string request_type)
    : EmbeddedPolicyTestServer::RequestHandler(parent),
      request_type_(std::move(request_type)) {}

RequestHandlerForChromeReport::~RequestHandlerForChromeReport() = default;

std::string RequestHandlerForChromeReport::RequestType() {
  return request_type_;
}

std::unique_ptr<net::test_server::HttpResponse>
RequestHandlerForChromeReport::HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::string device_id =
      KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID);
  enterprise_management::DeviceManagementRequest device_management_request;
  if (!device_management_request.ParseFromString(request.content)) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Failed to parse DeviceManagementRequest.");
  }

  enterprise_management::DeviceManagementResponse response;
  if (request_type_ == dm_protocol::kValueRequestChromeDesktopReport) {
    if (!device_management_request.has_chrome_desktop_report_request()) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                                "Missing chrome_desktop_report_request.");
    }
    report_storage()->AddBrowserReport(
        std::move(device_id),
        device_management_request.chrome_desktop_report_request());
    response.mutable_chrome_desktop_report_response();
  } else {
    if (!device_management_request.has_chrome_profile_report_request()) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                                "Missing chrome_profile_report_request.");
    }
    report_storage()->AddProfileReport(
        std::move(device_id),
        device_management_request.chrome_profile_report_request());
    response.mutable_chrome_profile_report_response();
  }

  return CreateHttpResponse(net::HTTP_OK, response);
}

}  // namespace policy
