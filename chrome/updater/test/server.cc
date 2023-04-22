// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/server.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/updater/test/http_request.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace test {
namespace {

std::string SerializeRequest(HttpRequest& request) {
  std::vector<std::string> request_strs;

  request_strs.push_back("Request:");
  request_strs.push_back(
      base::StringPrintf("Path: %s", request.relative_url.c_str()));
  request_strs.push_back("Headers: {");
  for (auto& header : request.headers) {
    request_strs.push_back(base::StringPrintf(
        "    %s: %s", header.first.c_str(), header.second.c_str()));
  }
  request_strs.push_back("}");
  request_strs.push_back(
      base::StringPrintf("Content: %s", GetPrintableContent(request).c_str()));

  return base::JoinString(request_strs, "\n  ");
}

}  // namespace

ScopedServer::ScopedServer(
    scoped_refptr<IntegrationTestCommands> integration_test_commands)
    : test_server_(std::make_unique<net::test_server::EmbeddedTestServer>()),
      integration_test_commands_(integration_test_commands) {
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &ScopedServer::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE((test_server_handle_ = test_server_->StartAndReturnHandle()));

  integration_test_commands_->EnterTestMode(update_url(), crash_upload_url(),
                                            device_management_url());
}

ScopedServer::~ScopedServer() {
  for (const auto& request_matcher_group : request_matcher_groups_) {
    // Forces `request_matcher` to log to help debugging, unless the
    // matcher matches the empty request.
    ADD_FAILURE() << "Unmet expectation: ";
    base::ranges::for_each(request_matcher_group, [](request::Matcher matcher) {
      matcher.Run(HttpRequest());
    });
  }
}

void ScopedServer::ExpectOnce(request::MatcherGroup request_matcher_group,
                              const std::string& response_body) {
  request_matcher_groups_.push_back(std::move(request_matcher_group));
  response_bodies_.push_back(response_body);
}

std::unique_ptr<net::test_server::HttpResponse> ScopedServer::HandleRequest(
    const net::test_server::HttpRequest& req) {
  HttpRequest request(req);
  VLOG(0) << "Handle request at path:" << request.relative_url;
  VLOG(3) << SerializeRequest(request);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request_matcher_groups_.empty()) {
    VLOG(0) << "Unexpected request.";
    ADD_FAILURE() << "Unexpected " << SerializeRequest(request);
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  if (!base::ranges::all_of(request_matcher_groups_.front(),
                            [&request](request::Matcher matcher) {
                              return matcher.Run(request);
                            })) {
    VLOG(0) << "Request did not match.";
    ADD_FAILURE() << "Unmatched " << SerializeRequest(request);
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }
  response->set_code(net::HTTP_OK);
  response->set_content(response_bodies_.front());
  request_matcher_groups_.pop_front();
  response_bodies_.pop_front();
  return response;
}

}  // namespace test
}  // namespace updater
