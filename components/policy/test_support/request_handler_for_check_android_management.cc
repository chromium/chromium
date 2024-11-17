// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_check_android_management.h"

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

const char kManagedAuthToken[] = "managed-auth-token";
const char kUnmanagedAuthToken[] = "unmanaged-auth-token";

RequestHandlerForCheckAndroidManagement::
    RequestHandlerForCheckAndroidManagement(EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForCheckAndroidManagement::
    ~RequestHandlerForCheckAndroidManagement() = default;

std::string RequestHandlerForCheckAndroidManagement::RequestType() {
  return dm_protocol::kValueRequestCheckAndroidManagement;
}

std::unique_ptr<HttpResponse>
RequestHandlerForCheckAndroidManagement::HandleRequest(
    const HttpRequest& request) {
  std::string oauth_token;
  net::GetValueForKeyInQuery(request.GetURL(), dm_protocol::kParamOAuthToken,
                             &oauth_token);

  em::DeviceManagementResponse response;
  response.mutable_check_android_management_response();
  if (oauth_token == kManagedAuthToken) {
    return CreateHttpResponse(net::HTTP_CONFLICT, response);
  }
  if (oauth_token == kUnmanagedAuthToken) {
    return CreateHttpResponse(net::HTTP_OK, response);
  }
  return CreateHttpResponse(net::HTTP_FORBIDDEN, "Invalid OAuth token");
}

}  // namespace policy
