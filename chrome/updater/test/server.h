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
#include "chrome/updater/test/integration_test_commands.h"
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

class ScopedServer {
 public:
  // Defines a generic predicate to match expectations for a `request_body`.
  using RequestMatcherPredicate =
      base::RepeatingCallback<bool(const std::string& request_body)>;

  // Defines a sequence of predicates which all must pass in order to match
  // a request. This allows for combining several predicates when matching a
  // single request.
  using RequestMatcher = std::list<RequestMatcherPredicate>;

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
  // expectation defined by applying all individual request matching predicates
  // composing the `request_matcher` in the order the expectations were set.
  // The server replies with an HTTP 200 and `response_body` to an expected
  // request. It replies with HTTP 500 and fails the test if a request does
  // not match the next expected `request_matcher`, or if there are no more
  // expected requests. If the server does not receive every expected request,
  // it will fail the test during destruction.
  void ExpectOnce(RequestMatcher request_matcher,
                  const std::string& response_body);

  const GURL& base_url() const { return test_server_->base_url(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::list<RequestMatcher> request_matchers_;
  std::list<std::string> response_bodies_;
  scoped_refptr<IntegrationTestCommands> integration_test_commands_;
};

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_SERVER_H_
