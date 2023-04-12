// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_SERVER_H_
#define CHROME_UPDATER_TEST_SERVER_H_

#include <list>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/test/request_matcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;

namespace net {
namespace test_server {

struct HttpRequest;
class HttpResponse;

}  // namespace test_server
}  // namespace net

namespace updater {
namespace test {

class IntegrationTestCommands;

class ScopedServer {
 public:
  // Creates and starts a scoped server. Sets up the updater to communicate
  // with it. Multiple scoped servers are not allowed.
  explicit ScopedServer(
      scoped_refptr<IntegrationTestCommands> integration_test_commands);

  // Shuts down the server and verifies that all expectations were met and that
  // no extra communications were received.
  ~ScopedServer();

  ScopedServer(const ScopedServer&) = delete;
  ScopedServer& operator=(const ScopedServer&) = delete;

  // Registers an expected request with the server. Requests must match the
  // expectation defined by applying all individual request matchers composing
  // the `request_matcher_group` in the order the expectations were set.
  // The server replies with an HTTP 200 and `response_body` to an expected
  // request. It replies with HTTP 500 and fails the test if a request does
  // not match the next expected `request_matcher_group`, or if there are no
  // more expected requests. If the server does not receive every expected
  // request, it will fail the test during destruction.
  void ExpectOnce(request::MatcherGroup request_matcher_group,
                  const std::string& response_body);

  std::string update_path() const { return "/update"; }
  GURL update_url() const { return test_server_->GetURL(update_path()); }

  std::string crash_report_path() const { return "/crash"; }
  GURL crash_upload_url() const {
    return test_server_->GetURL(crash_report_path());
  }

  std::string device_management_path() const { return "/dmapi"; }
  GURL device_management_url() const {
    return test_server_->GetURL(device_management_path());
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::list<request::MatcherGroup> request_matcher_groups_;
  std::list<std::string> response_bodies_;
  scoped_refptr<IntegrationTestCommands> integration_test_commands_;
};

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_SERVER_H_
