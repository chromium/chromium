// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_upload_euicc_info.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForUploadEuiccInfo::RequestHandlerForUploadEuiccInfo(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForUploadEuiccInfo::~RequestHandlerForUploadEuiccInfo() = default;

std::string RequestHandlerForUploadEuiccInfo::RequestType() {
  return dm_protocol::kValueRequestUploadEuiccInfo;
}

std::unique_ptr<HttpResponse> RequestHandlerForUploadEuiccInfo::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementResponse response;
  response.mutable_device_status_report_response();
  response.mutable_session_status_report_response();
  return CreateHttpResponse(net::HTTP_OK, response);
}

}  // namespace policy
