// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/server.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  for (const auto& request_matcher : request_matchers_) {
    // Forces `request_matcher` to log to help debugging, unless the
    // predicate matches "..." string in the request.
    ADD_FAILURE() << "Unmet expectation: ";
    std::for_each(request_matcher.begin(), request_matcher.end(),
                  [](RequestMatcherPredicate pred) { pred.Run("..."); });
  }
}

void ScopedServer::ExpectOnce(RequestMatcher request_matcher,
                              const std::string& response_body) {
  request_matchers_.push_back(std::move(request_matcher));
  response_bodies_.push_back(response_body);
}

std::unique_ptr<net::test_server::HttpResponse> ScopedServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  VLOG(0) << "HandleRequest: " << request.content;
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request_matchers_.empty()) {
    ADD_FAILURE() << "Unexpected request: " << request.content;
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  if (!std::all_of(request_matchers_.front().begin(),
                   request_matchers_.front().end(),
                   [&request](RequestMatcherPredicate pred) {
                     return pred.Run(request.content);
                   })) {
    ADD_FAILURE() << "Request did not match: " << request.content;
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  response->set_code(net::HTTP_OK);
  response->set_content(response_bodies_.front());
  request_matchers_.pop_front();
  response_bodies_.pop_front();
  return response;
}

}  // namespace test
}  // namespace updater
