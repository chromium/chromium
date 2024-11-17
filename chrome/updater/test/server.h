// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_SERVER_H_
#define CHROME_UPDATER_TEST_SERVER_H_

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/updater/test/request_matcher.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;

namespace net {
namespace test_server {

struct HttpRequest;
class HttpResponse;

}  // namespace test_server
}  // namespace net

namespace updater::test {

class IntegrationTestCommands;

class ScopedServer {
 public:
  // Creates and starts a scoped server. Sets up the updater to communicate
  // with it (see `ConfigureTestMode`). Multiple scoped servers are not allowed.
  // The provided `IntegrationTestCommands` object is not retained.
  explicit ScopedServer(
      scoped_refptr<IntegrationTestCommands> integration_test_commands);

  // Creates and starts a scoped server, without configuring any updater to
  // communicate with it. (`ConfigureTestMode` can be used later to do this.)
  // Multiple scoped servers are not allowed.
  ScopedServer();

  // Shuts down the server and verifies that all expectations were met and that
  // no extra communications were received.
  ~ScopedServer();

  ScopedServer(const ScopedServer&) = delete;
  ScopedServer& operator=(const ScopedServer&) = delete;

  std::string host_port_pair() const {
    return test_server_->host_port_pair().ToString();
  }

  // Configures the update service constants for the updater represented
  // by the provided IntegrationTestCommands object to send updates to this
  // `ScopedServer` instance. The configuration continues to persist after this
  // `ScopedServer` is destroyed.
  //
  // Multiple instances of `IntegrationTestCommands` can be configured to use
  // the same `ScopedServer` simultaneously.
  void ConfigureTestMode(IntegrationTestCommands* commands);

  // Registers an expected request with the server. Requests must match the
  // expectation defined by applying all individual request matchers composing
  // the `request_matcher_group` in the order the expectations were set.
  // The server replies with an HTTP 200 and `response_body` to an expected
  // request. It replies with HTTP 500 and fails the test if a request does
  // not match the next expected `request_matcher_group`, or if there are no
  // more expected requests. If the server does not receive every expected
  // request, it will fail the test during destruction.
  void ExpectOnce(request::MatcherGroup request_matcher_group,
                  const std::string& response_body,
                  net::HttpStatusCode response_status_code = net::HTTP_OK);

  GURL base_url() const { return test_server_->base_url(); }

  std::string update_path() const { return "/update"; }
  GURL update_url() const { return test_server_->GetURL(update_path()); }

  std::string download_path() const { return "/download"; }
  GURL download_url() const { return test_server_->GetURL(download_path()); }
  void set_download_delay(base::TimeDelta delay) { download_delay_ = delay; }

  std::string crash_report_path() const { return "/crash"; }
  GURL crash_upload_url() const {
    return test_server_->GetURL(crash_report_path());
  }

  std::string device_management_path() const { return "/dmapi"; }
  GURL device_management_url() const {
    return test_server_->GetURL(device_management_path());
  }

  std::string app_logo_path() const { return "/applogo/"; }
  GURL app_logo_url() const { return test_server_->GetURL(app_logo_path()); }

  std::string proxy_pac_path() const { return "/pac_script.pac"; }
  GURL proxy_pac_url() const { return test_server_->GetURL(proxy_pac_path()); }

  std::string proxy_url_no_path() const {
    std::string proxy = test_server_->base_url().spec();
    // A valid proxy string should not have any path component. Strip the root
    // path ('/') if it is present.
    if (proxy.back() == '/') {
      proxy.pop_back();
    }
    return proxy;
  }

  bool gzip_response() const { return gzip_response_; }
  void set_gzip_response(bool gzip_response) { gzip_response_ = gzip_response; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_ =
      std::make_unique<net::test_server::EmbeddedTestServer>();
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  std::list<request::MatcherGroup> request_matcher_groups_;
  std::list<std::pair<net::HttpStatusCode, std::string>> responses_;
  base::TimeDelta download_delay_;
  bool gzip_response_ = false;
};

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_SERVER_H_
