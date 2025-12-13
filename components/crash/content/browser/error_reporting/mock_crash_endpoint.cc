// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr const char* kTestCrashEndpoint = "/crash";
}

class MockCrashEndpoint::Client : public crash_reporter::CrashReporterClient {
 public:
  explicit Client(MockCrashEndpoint* owner) : owner_(owner) {}

  bool GetCollectStatsConsent() override {
    // In production, GetCollectStatsConsent may be blocking due to file reads.
    // Simulate this in our tests as well.
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    return owner_->consented_;
  }
  void GetProductInfo(ProductInfo* product_info) override {
    product_info->product_name = "Chrome_ChromeOS";
    product_info->version = "1.2.3.4";
    product_info->channel = "Stable";
  }
 private:
  raw_ptr<MockCrashEndpoint> owner_;
};

MockCrashEndpoint::Report::Report(
    std::multimap<std::string, std::string> query_params,
    std::string content)
    : query_params_(std::move(query_params)), content_(std::move(content)) {}

MockCrashEndpoint::Report::Report(const MockCrashEndpoint::Report&) = default;
MockCrashEndpoint::Report::~Report() = default;

MockCrashEndpoint::MockCrashEndpoint(
    net::test_server::EmbeddedTestServer* test_server)
    : test_server_(test_server) {
  test_server->RegisterRequestHandler(base::BindRepeating(
      &MockCrashEndpoint::HandleRequest, base::Unretained(this)));
  EXPECT_TRUE(test_server->Start());

  client_ = std::make_unique<Client>(this);
  crash_reporter::SetCrashReporterClient(client_.get());
}

MockCrashEndpoint::~MockCrashEndpoint() {
  crash_reporter::SetCrashReporterClient(nullptr);
}

std::string MockCrashEndpoint::GetCrashEndpointURL() const {
  return test_server_->GetURL(kTestCrashEndpoint).spec();
}

MockCrashEndpoint::Report MockCrashEndpoint::WaitForReport() {
  if (last_report_) {
    return *last_report_;
  }
  base::RunLoop run_loop;
  on_report_ = run_loop.QuitClosure();
  run_loop.Run();
  on_report_.Reset();
  return *last_report_;
}

// static
MockCrashEndpoint::Report MockCrashEndpoint::Report::ParseQuery(
    std::string_view query,
    std::string content) {
  base::StringPairs param_pairs;
  // Tolerate and discard empty key-value pairs.
  base::SplitStringIntoKeyValuePairs(query, '=', '&', &param_pairs);
  std::multimap<std::string, std::string> query_params;
  for (const auto& param : param_pairs) {
    std::string unescaped_value = base::UnescapeURLComponent(
        param.second,
        base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
            base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
            base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
    query_params.emplace(param.first, std::move(unescaped_value));
  }
  return MockCrashEndpoint::Report(std::move(query_params), std::move(content));
}

std::optional<std::string_view> MockCrashEndpoint::Report::GetQueryParam(
    const std::string& param) const {
  auto it = query_params_.find(param);
  if (it != query_params_.end()) {
    return it->second;
  } else {
    return std::nullopt;
  }
}

std::unique_ptr<net::test_server::HttpResponse>
MockCrashEndpoint::HandleRequest(const net::test_server::HttpRequest& request) {
  GURL absolute_url = test_server_->GetURL(request.relative_url);
  LOG(INFO) << "MockCrashEndpoint::HandleRequest(" << absolute_url.spec()
            << ")";
  if (absolute_url.GetPath() != kTestCrashEndpoint) {
    return nullptr;
  }

  ++report_count_;
  last_report_ = Report::ParseQuery(absolute_url.GetQuery(), request.content);
  all_reports_.push_back(*last_report_);
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(response_code_);
  http_response->set_content(response_content_);
  http_response->set_content_type("text/plain");
  if (on_report_) {
    on_report_.Run();
  }
  return http_response;
}

std::ostream& operator<<(std::ostream& out,
                         const MockCrashEndpoint::Report& report) {
  std::vector<std::string> param_pairs;
  for (const auto& pair : report.query_params()) {
    std::string escaped_value =
        base::EscapeQueryParamValue(pair.second, /*use_plus=*/true);
    param_pairs.push_back(
        base::StrCat({pair.first, "=", std::move(escaped_value)}));
  }
  std::string query = base::JoinString(param_pairs, "&");
  out << "query: " << std::move(query) << "\ncontent: " << report.content();
  return out;
}
