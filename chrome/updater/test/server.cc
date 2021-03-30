// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/server.h"

#include <list>
#include <memory>
#include <string>

#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace updater {
namespace test {

ScopedServer::ScopedServer(
    scoped_refptr<IntegrationTestCommands> integration_test_commands)
    : test_server_(std::make_unique<net::test_server::EmbeddedTestServer>()),
      integration_test_commands_(integration_test_commands) {
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &ScopedServer::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));

  integration_test_commands_->EnterTestMode(test_server_->base_url());
}

ScopedServer::~ScopedServer() {
  for (const auto& regex : request_body_regexes_) {
    ADD_FAILURE() << "Unmet expectation: " << regex;
  }
}

void ScopedServer::ExpectOnce(const std::string& request_body_regex,
                              const std::string& response_body) {
  request_body_regexes_.push_back(request_body_regex);
  response_bodies_.push_back(response_body);
}

std::unique_ptr<net::test_server::HttpResponse> ScopedServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request_body_regexes_.empty()) {
    ADD_FAILURE() << "Unexpected request with body: " << request.content;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  if (!re2::RE2::PartialMatch(request.content, request_body_regexes_.front())) {
    ADD_FAILURE() << "Request with body: " << request.content
                  << " did not match expected regex "
                  << request_body_regexes_.front();
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_bodies_.front());
  request_body_regexes_.pop_front();
  response_bodies_.pop_front();
  return response;
}

}  // namespace test
}  // namespace updater
