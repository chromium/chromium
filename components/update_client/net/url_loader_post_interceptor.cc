// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/net/url_loader_post_interceptor.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "components/update_client/test_configurator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

URLLoaderPostInterceptor::URLLoaderPostInterceptor(
    network::TestURLLoaderFactory* url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  filtered_urls_.push_back(
      GURL(base::StringPrintf("%s://%s%s", POST_INTERCEPT_SCHEME,
                              POST_INTERCEPT_HOSTNAME, POST_INTERCEPT_PATH)));
  InitializeWithInterceptor();
}

URLLoaderPostInterceptor::URLLoaderPostInterceptor(
    std::vector<GURL> supported_urls,
    network::TestURLLoaderFactory* url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  CHECK_LT(0u, supported_urls.size());
  filtered_urls_.swap(supported_urls);
  InitializeWithInterceptor();
}

URLLoaderPostInterceptor::URLLoaderPostInterceptor(
    std::vector<GURL> supported_urls,
    net::test_server::EmbeddedTestServer* embedded_test_server)
    : embedded_test_server_(embedded_test_server) {
  CHECK_LT(0u, supported_urls.size());
  filtered_urls_.swap(supported_urls);
  InitializeWithRequestHandler();
}

URLLoaderPostInterceptor::~URLLoaderPostInterceptor() = default;

bool URLLoaderPostInterceptor::ExpectRequest(
    std::unique_ptr<RequestMatcher> request_matcher) {
  return ExpectRequest(std::move(request_matcher), net::HTTP_OK);
}

bool URLLoaderPostInterceptor::ExpectRequest(
    std::unique_ptr<RequestMatcher> request_matcher,
    net::HttpStatusCode response_code) {
  expectations_.emplace(std::move(request_matcher),
                        ExpectationResponse(response_code, ""));
  return true;
}

bool URLLoaderPostInterceptor::ExpectRequest(
    std::unique_ptr<RequestMatcher> request_matcher,
    const base::FilePath& filepath) {
  std::string response;
  if (filepath.empty() || !base::ReadFileToString(filepath, &response)) {
    return false;
  }
  expectations_.emplace(std::move(request_matcher),
                        ExpectationResponse(net::HTTP_OK, response));
  return true;
}

// Returns how many requests have been intercepted and matched by
// an expectation. One expectation can only be matched by one request.
int URLLoaderPostInterceptor::GetHitCount() const {
  return hit_count_;
}

// Returns how many requests in total have been captured by the interceptor.
int URLLoaderPostInterceptor::GetCount() const {
  return static_cast<int>(requests_.size());
}

// Returns all requests that have been intercepted, matched or not.
std::vector<URLLoaderPostInterceptor::InterceptedRequest>
URLLoaderPostInterceptor::GetRequests() const {
  return requests_;
}

// Return the body of the n-th request, zero-based.
std::string URLLoaderPostInterceptor::GetRequestBody(size_t n) const {
  return std::get<0>(requests_[n]);
}

// Returns the joined bodies of all requests for debugging purposes.
std::string URLLoaderPostInterceptor::GetRequestsAsString() const {
  const std::vector<InterceptedRequest> requests = GetRequests();
  std::string s = "Requests are:";
  int i = 0;
  for (auto it = requests.cbegin(); it != requests.cend(); ++it) {
    s.append(base::StringPrintf("\n  [%d]: %s", ++i, std::get<0>(*it).c_str()));
  }
  return s;
}

// Resets the state of the interceptor so that new expectations can be set.
void URLLoaderPostInterceptor::Reset() {
  hit_count_ = 0;
  requests_.clear();
  base::queue<Expectation>().swap(expectations_);
}

void URLLoaderPostInterceptor::Pause() {
  is_paused_ = true;
}

void URLLoaderPostInterceptor::Resume() {
  is_paused_ = false;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        if (pending_expectations_.empty()) {
          return;
        }
        const auto& [url, response] = pending_expectations_.front();
        url_loader_factory_->AddResponse(url.spec(), response.response_body,
                                         response.response_code);
        pending_expectations_.pop();
      }));
}

void URLLoaderPostInterceptor::url_job_request_ready_callback(
    UrlJobRequestReadyCallback url_job_request_ready_callback) {
  url_job_request_ready_callback_ = std::move(url_job_request_ready_callback);
}

int URLLoaderPostInterceptor::GetHitCountForURL(const GURL& url) {
  int hit_count = 0;
  const std::vector<InterceptedRequest> requests = GetRequests();
  for (auto it = requests.cbegin(); it != requests.cend(); ++it) {
    GURL url_no_query = std::get<2>(*it);
    if (url_no_query.has_query()) {
      GURL::Replacements replacements;
      replacements.ClearQuery();
      url_no_query = url_no_query.ReplaceComponents(replacements);
    }
    if (url_no_query == url) {
      hit_count++;
    }
  }
  return hit_count;
}

void URLLoaderPostInterceptor::InitializeWithInterceptor() {
  CHECK(url_loader_factory_);
  url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        GURL url = request.url;
        if (url.has_query()) {
          GURL::Replacements replacements;
          replacements.ClearQuery();
          url = url.ReplaceComponents(replacements);
        }
        if (!base::Contains(filtered_urls_, url)) {
          return;
        }

        std::string request_body = network::GetUploadData(request);
        requests_.emplace_back(request_body, request.headers, request.url);
        if (expectations_.empty()) {
          return;
        }
        const auto& [matcher, response] = expectations_.front();
        if (matcher->Match(request_body)) {
          const net::HttpStatusCode response_code(response.response_code);
          const std::string response_body(response.response_body);

          if (url_job_request_ready_callback_) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, std::move(url_job_request_ready_callback_));
          }

          if (!is_paused_) {
            url_loader_factory_->AddResponse(request.url.spec(), response_body,
                                             response_code);
          } else {
            pending_expectations_.emplace(request.url, response);
          }
          expectations_.pop();
          ++hit_count_;
        }
      }));
}

void URLLoaderPostInterceptor::InitializeWithRequestHandler() {
  CHECK(embedded_test_server_);
  CHECK(!url_loader_factory_);
  embedded_test_server_->RegisterRequestHandler(base::BindRepeating(
      &URLLoaderPostInterceptor::RequestHandler, base::Unretained(this)));
}

std::unique_ptr<net::test_server::HttpResponse>
URLLoaderPostInterceptor::RequestHandler(
    const net::test_server::HttpRequest& request) {
  // Only intercepts POST.
  if (request.method != net::test_server::METHOD_POST) {
    return nullptr;
  }

  GURL url = request.GetURL();
  if (url.has_query()) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    url = url.ReplaceComponents(replacements);
  }
  if (!base::Contains(filtered_urls_, url)) {
    return nullptr;
  }

  std::string request_body = request.content;
  net::HttpRequestHeaders headers;
  for (const auto& [name, value] : request.headers) {
    headers.SetHeader(name, value);
  }
  requests_.emplace_back(request_body, headers, url);
  if (expectations_.empty()) {
    return nullptr;
  }

  const auto& [matcher, response] = expectations_.front();
  if (matcher->Match(request_body)) {
    const net::HttpStatusCode response_code(response.response_code);
    const std::string response_body(response.response_body);
    expectations_.pop();
    ++hit_count_;

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(response_code);
    http_response->set_content(response_body);
    return http_response;
  }

  return nullptr;
}

bool PartialMatch::Match(const std::string& actual) const {
  return actual.find(expected_) != std::string::npos;
}

bool AnyMatch::Match(const std::string& actual) const {
  return true;
}

}  // namespace update_client
