// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_SERVER_H_
#define CHROME_UPDATER_TEST_SERVER_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
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

struct HttpRequest;
class IntegrationTestCommands;

// A test HTTP server that answers expected requests and fails the test on any
// other request. Expectations are served in the order they are registered,
// except that entries registered by one `ExpectUnordered` call may be served
// in any order among themselves. A request that matches no expectation due
// next gets HTTP 500 and fails the test. Matchers and response body providers
// run on the server's IO thread. Matchers must not call into the
// `ScopedServer`; response body providers may.
class ScopedServer {
 public:
  // An expected request and the response to it. A request matches if every
  // matcher in `matchers` succeeds. `response_body_provider` receives `true`
  // for protocol 4.0 requests.
  struct Expectation {
    Expectation(request::MatcherGroup matchers,
                std::string response_body,
                net::HttpStatusCode response_status_code = net::HTTP_OK);
    Expectation(
        request::MatcherGroup matchers,
        base::RepeatingCallback<std::string(bool)> response_body_provider,
        net::HttpStatusCode response_status_code = net::HTTP_OK);
    Expectation(const Expectation&);
    Expectation& operator=(const Expectation&);
    Expectation(Expectation&&);
    Expectation& operator=(Expectation&&);
    ~Expectation();

    request::MatcherGroup matchers;
    base::RepeatingCallback<std::string(bool)> response_body_provider;
    net::HttpStatusCode response_status_code;
  };

  // Creates and starts a scoped server without setting up an updater to use
  // it.
  ScopedServer();

  // Creates and starts a scoped server. Sets up the updater to communicate with
  // it. The configuration persists after this instance is destroyed.
  explicit ScopedServer(scoped_refptr<IntegrationTestCommands> commands);

  // Shuts down the server and fails the test for each expectation that was
  // never matched.
  ~ScopedServer();

  ScopedServer(const ScopedServer&) = delete;
  ScopedServer& operator=(const ScopedServer&) = delete;

  std::string host_port_pair() const {
    return test_server_.host_port_pair().ToString();
  }

  // Registers one expected request. A request matches if every matcher in
  // `matchers` succeeds; the server then replies with `response_status_code`
  // and `response_body`.
  void ExpectOnce(request::MatcherGroup matchers,
                  const std::string& response_body,
                  net::HttpStatusCode response_status_code = net::HTTP_OK);

  // Same as above, but `response_body_provider` returns the response body. It
  // receives `true` for protocol 4.0 requests.
  void ExpectOnce(
      request::MatcherGroup matchers,
      base::RepeatingCallback<std::string(bool)> response_body_provider,
      net::HttpStatusCode response_status_code = net::HTTP_OK);

  // Registers `expectations` as one group whose requests may arrive in any
  // order. Each request takes the first entry it matches, so list overlapping
  // entries from most to least specific. Does nothing if `expectations` is
  // empty.
  void ExpectUnordered(std::vector<Expectation> expectations);

  GURL base_url() const { return test_server_.base_url(); }

  std::string update_path() const { return "/update"; }
  GURL update_url() const { return test_server_.GetURL(update_path()); }

  std::string download_path() const { return "/download"; }
  GURL download_url() const { return test_server_.GetURL(download_path()); }
  void set_download_delay(base::TimeDelta delay);

  std::string crash_report_path() const { return "/crash"; }
  GURL crash_upload_url() const {
    return test_server_.GetURL(crash_report_path());
  }

  std::string device_management_path() const { return "/dmapi"; }
  GURL device_management_url() const {
    return test_server_.GetURL(device_management_path());
  }

  std::string app_logo_path() const { return "/applogo/"; }
  GURL app_logo_url() const { return test_server_.GetURL(app_logo_path()); }

  std::string event_logging_path() const { return "/event_logging/"; }
  GURL event_logging_url() const {
    return test_server_.GetURL(event_logging_path());
  }

  std::string proxy_pac_path() const { return "/pac_script.pac"; }
  GURL proxy_pac_url() const { return test_server_.GetURL(proxy_pac_path()); }

  std::string proxy_url_no_path() const {
    std::string proxy = test_server_.base_url().spec();
    // A valid proxy string should not have any path component. Strip the root
    // path ('/') if it is present.
    if (proxy.back() == '/') {
      proxy.pop_back();
    }
    return proxy;
  }

  // If `gzip_response` is true, the server gzip-compresses response bodies and
  // fails the test on requests that do not accept gzip.
  void set_gzip_response(bool gzip_response);

 private:
  friend class ScopedServerTest;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  // Removes and returns the first entry of the front expectation group that
  // `request` matches. Fails the test and returns `std::nullopt` if no
  // expectations remain or no entry in the front group matches.
  std::optional<Expectation> TakeMatchingExpectation(const HttpRequest& request)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  net::test_server::EmbeddedTestServer test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  base::Lock lock_;
  std::list<std::vector<Expectation>> expectation_groups_ GUARDED_BY(lock_);
  base::TimeDelta download_delay_ GUARDED_BY(lock_);
  bool gzip_response_ GUARDED_BY(lock_) = false;
};

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_SERVER_H_
