// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/failing_request_handler.h"

#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

FailingRequestHandler::FailingRequestHandler(EmbeddedPolicyTestServer* parent,
                                             const std::string& request_type,
                                             net::HttpStatusCode error_code)
    : EmbeddedPolicyTestServer::RequestHandler(parent),
      request_type_(request_type),
      error_code_(error_code) {}

FailingRequestHandler::~FailingRequestHandler() = default;

std::string FailingRequestHandler::RequestType() {
  return request_type_;
}

std::unique_ptr<net::test_server::HttpResponse>
FailingRequestHandler::HandleRequest(
    const net::test_server::HttpRequest& request) {
  return CreateHttpResponse(error_code_, "Preconfigured error");
}

}  // namespace policy
