// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_fm_registration_token_upload.h"

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

RequestHandlerForFmRegistrationTokenUpload::
    RequestHandlerForFmRegistrationTokenUpload(EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForFmRegistrationTokenUpload::
    ~RequestHandlerForFmRegistrationTokenUpload() = default;

std::string RequestHandlerForFmRegistrationTokenUpload::RequestType() {
  return dm_protocol::kValueRequestFmRegistrationTokenUpload;
}

std::unique_ptr<HttpResponse>
RequestHandlerForFmRegistrationTokenUpload::HandleRequest(
    const HttpRequest& request) {
  return CreateHttpResponse(net::HTTP_OK, em::DeviceManagementResponse());
}

}  // namespace policy
