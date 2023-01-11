// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NET_URL_LOADER_POST_INTERCEPTOR_H_
#define COMPONENTS_UPDATE_CLIENT_NET_URL_LOADER_POST_INTERCEPTOR_H_

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace network {
class TestURLLoaderFactory;
}

namespace net {
namespace test_server {
class EmbeddedTestServer;
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace update_client {

// Intercepts requests to a file path, counts them, and captures the body of
// the requests. Optionally, for each request, it can return a canned response
// from a given file. The class maintains a queue of expectations, and returns
// one and only one response for each request that matches the expectation.
// Then, the expectation is removed from the queue.
class URLLoaderPostInterceptor {
 public:
  using InterceptedRequest =
      std::tuple<std::string, net::HttpRequestHeaders, GURL>;

  // Called when the load associated with the url request is intercepted
  // by this object:.
  using UrlJobRequestReadyCallback = base::OnceCallback<void()>;

  // Allows a generic string maching interface when setting up expectations.
  class RequestMatcher {
   public:
    virtual bool Match(const std::string& actual) const = 0;
    virtual ~RequestMatcher() = default;
  };

  explicit URLLoaderPostInterceptor(
      network::TestURLLoaderFactory* url_loader_factory);
  URLLoaderPostInterceptor(std::vector<GURL> supported_urls,
                           network::TestURLLoaderFactory* url_loader_factory);
  URLLoaderPostInterceptor(std::vector<GURL> supported_urls,
                           net::test_server::EmbeddedTestServer*);

  URLLoaderPostInterceptor(const URLLoaderPostInterceptor&) = delete;
  URLLoaderPostInterceptor& operator=(const URLLoaderPostInterceptor&) = delete;

  ~URLLoaderPostInterceptor();

  // Sets an expection for the body of the POST request and optionally,
  // provides a canned response identified by a |file_path| to be returned when
  // the expectation is met. If no |file_path| is provided, then an empty
  // response body is served. If |response_code| is provided, then an empty
  // response body with that response code is returned.
  // Returns |true| if the expectation was set.
  bool ExpectRequest(std::unique_ptr<RequestMatcher> request_matcher);

  bool ExpectRequest(std::unique_ptr<RequestMatcher> request_matcher,
                     net::HttpStatusCode response_code);

  bool ExpectRequest(std::unique_ptr<RequestMatcher> request_matcher,
                     const base::FilePath& filepath);

  // Returns how many requests have been intercepted and matched by
  // an expectation. One expectation can only be matched by one request.
  int GetHitCount() const;

  // Returns how many requests in total have been captured by the interceptor.
  int GetCount() const;

  // Returns all requests that have been intercepted, matched or not.
  std::vector<InterceptedRequest> GetRequests() const;

  // Return the body of the n-th request, zero-based.
  std::string GetRequestBody(size_t n) const;

  // Returns the joined bodies of all requests for debugging purposes.
  std::string GetRequestsAsString() const;

  // Resets the state of the interceptor so that new expectations can be set.
  void Reset();

  // Prevents the intercepted request from starting, as a way to simulate
  // the effects of a very slow network. Call this function before the actual
  // network request occurs.
  void Pause();

  // Allows a previously paused request to continue.
  void Resume();

  // Sets a callback to be invoked when the request job associated with
  // an intercepted request is created. This allows the test execution to
  // synchronize with network tasks running on the IO thread and avoid polling
  // using idle run loops. A paused request can be resumed after this callback
  // has been invoked.
  void url_job_request_ready_callback(
      UrlJobRequestReadyCallback url_job_request_ready_callback);

  int GetHitCountForURL(const GURL& url);

 private:
  void InitializeWithInterceptor();
  void InitializeWithRequestHandler();

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request);

  struct ExpectationResponse {
    ExpectationResponse(net::HttpStatusCode code, const std::string& body)
        : response_code(code), response_body(body) {}
    const net::HttpStatusCode response_code;
    const std::string response_body;
  };
  using Expectation =
      std::pair<std::unique_ptr<RequestMatcher>, ExpectationResponse>;

  using PendingExpectation = std::pair<GURL, ExpectationResponse>;

  // Contains the count of the request matching expectations.
  int hit_count_ = 0;

  // Contains the request body and the extra headers of the intercepted
  // requests.
  std::vector<InterceptedRequest> requests_;

  // Contains the expectations which this interceptor tries to match.
  base::queue<Expectation> expectations_;

  base::queue<PendingExpectation> pending_expectations_;

  raw_ptr<network::TestURLLoaderFactory> url_loader_factory_ = nullptr;
  raw_ptr<net::test_server::EmbeddedTestServer> embedded_test_server_ = nullptr;

  bool is_paused_ = false;

  std::vector<GURL> filtered_urls_;

  UrlJobRequestReadyCallback url_job_request_ready_callback_;
};

class PartialMatch : public URLLoaderPostInterceptor::RequestMatcher {
 public:
  explicit PartialMatch(const std::string& expected) : expected_(expected) {}

  PartialMatch(const PartialMatch&) = delete;
  PartialMatch& operator=(const PartialMatch&) = delete;

  bool Match(const std::string& actual) const override;

 private:
  const std::string expected_;
};

class AnyMatch : public URLLoaderPostInterceptor::RequestMatcher {
 public:
  AnyMatch() = default;

  AnyMatch(const AnyMatch&) = delete;
  AnyMatch& operator=(const AnyMatch&) = delete;

  bool Match(const std::string& actual) const override;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_URL_LOADER_POST_INTERCEPTOR_H_
