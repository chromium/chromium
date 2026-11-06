// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/server.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/updater/test/http_request.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/request_matcher.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"

namespace updater::test {
namespace {

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_X86_64) && !defined(NDEBUG)
// Debug x86_64 binaries exceed 600 MB, causing slow CECA launch due to
// Gatekeeper verification and Rosetta translation.
constexpr base::TimeDelta kCecaConnectionTimeout = base::Seconds(50);
#else
constexpr base::TimeDelta kCecaConnectionTimeout = base::Seconds(10);
#endif

std::string DebugStringForReq(const HttpRequest& request) {
  std::vector<std::string> request_strs;

  request_strs.push_back("Request:");
  request_strs.push_back(absl::StrFormat("Path: %s", request.relative_url));
  request_strs.push_back("Headers: {");
  for (const auto& [name, value] : request.headers) {
    request_strs.push_back(absl::StrFormat("    %s: %s", name, value));
  }
  request_strs.push_back("}");

  std::optional<base::Value> json = base::JSONReader::Read(
      request.decoded_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (json) {
    request_strs.push_back(absl::StrFormat("Content (JSON, pretty-printed): %s",
                                           json->DebugString()));
  } else {
    request_strs.push_back(
        absl::StrFormat("Content: %s", GetPrintableContent(request)));
  }

  return base::JoinString(request_strs, "\n  ");
}

// Returns the first failure of `matchers` on `request`, or success if all
// matchers succeed.
testing::AssertionResult MatchAll(const request::MatcherGroup& matchers,
                                  const HttpRequest& request) {
  for (const request::Matcher& matcher : matchers) {
    if (testing::AssertionResult result = matcher.Run(request); !result) {
      return result;
    }
  }
  return testing::AssertionSuccess();
}

std::unique_ptr<net::test_server::HttpResponse> MakeErrorResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
  return response;
}

bool AcceptsGzip(const HttpRequest& request) {
  auto it = request.headers.find("Accept-Encoding");
  return it != request.headers.end() &&
         it->second.find("gzip") != std::string::npos;
}

// Returns whether the decoded request body has a `"protocol": "4.0"` entry,
// with any number of spaces after the colon.
bool IsProtocolV4(const HttpRequest& request) {
  return re2::RE2::PartialMatch(request.decoded_content,
                                R"("protocol": *"4\.0")");
}

}  // namespace

ScopedServer::Expectation::Expectation(request::MatcherGroup matchers,
                                       std::string response_body,
                                       net::HttpStatusCode response_status_code)
    : Expectation(
          std::move(matchers),
          base::BindRepeating([](const std::string& response_body,
                                 bool /*is_v4*/) { return response_body; },
                              std::move(response_body)),
          response_status_code) {}

ScopedServer::Expectation::Expectation(
    request::MatcherGroup matchers,
    base::RepeatingCallback<std::string(bool)> response_body_provider,
    net::HttpStatusCode response_status_code)
    : matchers(std::move(matchers)),
      response_body_provider(std::move(response_body_provider)),
      response_status_code(response_status_code) {}

ScopedServer::Expectation::Expectation(const Expectation&) = default;
ScopedServer::Expectation& ScopedServer::Expectation::operator=(
    const Expectation&) = default;
ScopedServer::Expectation::Expectation(Expectation&&) = default;
ScopedServer::Expectation& ScopedServer::Expectation::operator=(Expectation&&) =
    default;
ScopedServer::Expectation::~Expectation() = default;

ScopedServer::ScopedServer() {
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &ScopedServer::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE((test_server_handle_ = test_server_.StartAndReturnHandle()));
}

ScopedServer::ScopedServer(scoped_refptr<IntegrationTestCommands> commands)
    : ScopedServer() {
  CHECK(commands);
  commands->EnterTestMode(update_url(), crash_upload_url(),
                          /*app_logo_url=*/{},
                          /*event_logging_url=*/{}, base::Minutes(5),
                          base::Seconds(2), kCecaConnectionTimeout,
                          /*event_logging_permission_provider=*/std::nullopt);
}

ScopedServer::~ScopedServer() {
  // Stop the server: `HandleRequest` must not run during the checks below or
  // after the members it uses are destroyed. Stop it before taking `lock_`:
  // stopping waits for an in-flight `HandleRequest`, which takes `lock_`.
  test_server_handle_ = net::test_server::EmbeddedTestServerHandle();
  base::AutoLock lock(lock_);
  for (const std::vector<Expectation>& group : expectation_groups_) {
    for (const Expectation& expectation : group) {
      // Probes each matcher with an empty request to collect failure messages.
      std::vector<std::string> unmet_reasons;
      for (const request::Matcher& matcher : expectation.matchers) {
        if (testing::AssertionResult result = matcher.Run(HttpRequest());
            !result) {
          unmet_reasons.push_back(result.message());
        }
      }
      ADD_FAILURE() << "Unmet expectation:\n  "
                    << base::JoinString(unmet_reasons, "\n  ");
    }
  }
}

void ScopedServer::ExpectOnce(request::MatcherGroup matchers,
                              const std::string& response_body,
                              net::HttpStatusCode response_status_code) {
  ExpectUnordered({{std::move(matchers), response_body, response_status_code}});
}

void ScopedServer::ExpectOnce(
    request::MatcherGroup matchers,
    base::RepeatingCallback<std::string(bool)> response_body_provider,
    net::HttpStatusCode response_status_code) {
  ExpectUnordered({{std::move(matchers), std::move(response_body_provider),
                    response_status_code}});
}

void ScopedServer::ExpectUnordered(std::vector<Expectation> expectations) {
  // An empty group would never be matched or removed.
  if (expectations.empty()) {
    return;
  }
  base::AutoLock lock(lock_);
  expectation_groups_.push_back(std::move(expectations));
}

void ScopedServer::set_download_delay(base::TimeDelta delay) {
  base::AutoLock lock(lock_);
  download_delay_ = delay;
}

void ScopedServer::set_gzip_response(bool gzip_response) {
  base::AutoLock lock(lock_);
  gzip_response_ = gzip_response;
}

std::optional<ScopedServer::Expectation> ScopedServer::TakeMatchingExpectation(
    const HttpRequest& request) {
  if (expectation_groups_.empty()) {
    ADD_FAILURE() << "Unexpected " << DebugStringForReq(request);
    return std::nullopt;
  }
  std::vector<Expectation>& group = expectation_groups_.front();
  std::vector<std::string> failures;
  for (auto it = group.begin(); it != group.end(); ++it) {
    if (testing::AssertionResult result = MatchAll(it->matchers, request);
        !result) {
      failures.push_back(result.message());
      continue;
    }
    Expectation expectation = std::move(*it);
    group.erase(it);
    if (group.empty()) {
      expectation_groups_.pop_front();
    }
    return expectation;
  }
  ADD_FAILURE() << "Unmatched " << DebugStringForReq(request)
                << "\nCandidate expectation failures:\n  "
                << base::JoinString(failures, "\n  ");
  return std::nullopt;
}

std::unique_ptr<net::test_server::HttpResponse> ScopedServer::HandleRequest(
    const net::test_server::HttpRequest& req) {
  const HttpRequest request(req);
  VLOG(0) << "Handle request at path: " << request.relative_url;
  VLOG(3) << DebugStringForReq(request);

  std::optional<Expectation> expectation;
  base::TimeDelta download_delay;
  bool gzip_response = false;
  {
    base::AutoLock lock(lock_);
    gzip_response = gzip_response_;
    // Check before matching, so a rejected request keeps its expectation.
    if (gzip_response && !AcceptsGzip(request)) {
      ADD_FAILURE() << "gzip `Accept-Encoding` not found in request, "
                    << DebugStringForReq(request);
      return MakeErrorResponse();
    }
    expectation = TakeMatchingExpectation(request);
    download_delay = download_delay_;
  }
  if (!expectation) {
    return MakeErrorResponse();
  }

  // Runs without holding `lock_`, so the provider may call into this server.
  std::string response_body =
      expectation->response_body_provider.Run(IsProtocolV4(request));
  if (request.relative_url.starts_with(proxy_pac_path())) {
    VLOG(1) << "PAC proxy settings: [ " << response_body << "]";
  }
  if (gzip_response) {
    std::string compressed_body;
    if (!compression::GzipCompress(response_body, &compressed_body)) {
      ADD_FAILURE() << "gzip compression failed, "
                    << DebugStringForReq(request);
      return MakeErrorResponse();
    }
    response_body = std::move(compressed_body);
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url.starts_with(download_path()) &&
      !download_delay.is_zero()) {
    VLOG(0) << "Delay download response by: " << download_delay;
    response =
        std::make_unique<net::test_server::DelayedHttpResponse>(download_delay);
  } else {
    response = std::make_unique<net::test_server::BasicHttpResponse>();
  }
  response->set_code(expectation->response_status_code);
  if (request.relative_url.starts_with(device_management_path())) {
    response->set_content_type("application/x-protobuf");
  }
  if (gzip_response) {
    response->AddCustomHeader("Content-Encoding", "gzip");
  }
  response->set_content(response_body);
  return response;
}

}  // namespace updater::test
