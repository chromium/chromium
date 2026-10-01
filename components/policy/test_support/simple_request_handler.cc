// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/simple_request_handler.h"

#include <utility>

#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace policy {

SimpleRequestHandler::SimpleRequestHandler(
    EmbeddedPolicyTestServer* parent,
    std::string request_type,
    enterprise_management::DeviceManagementResponse response)
    : EmbeddedPolicyTestServer::RequestHandler(parent),
      request_type_(std::move(request_type)),
      response_(std::move(response)) {}

SimpleRequestHandler::~SimpleRequestHandler() = default;

std::string SimpleRequestHandler::RequestType() {
  return request_type_;
}

std::unique_ptr<HttpResponse> SimpleRequestHandler::HandleRequest(
    const HttpRequest& request) {
  return CreateHttpResponse(net::HTTP_OK, response_);
}

}  // namespace policy
