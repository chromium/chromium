// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_redirect/subresource_redirect_browser_test_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/subresource_redirect/proto/robots_rules.pb.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_redirect {

void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count) {
  while (true) {
    FetchHistogramsFromChildProcesses();

    const std::vector<base::Bucket> buckets =
        histogram_tester->GetAllSamples(histogram_name);
    size_t total_count = 0;
    for (const auto& bucket : buckets) {
      total_count += bucket.count;
    }
    if (total_count >= count) {
      break;
    }
  }
}

void FetchHistogramsFromChildProcesses() {
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}

RobotsRulesTestServer::RobotsRulesTestServer()
    : server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

RobotsRulesTestServer::~RobotsRulesTestServer() = default;

bool RobotsRulesTestServer::Start() {
  server_.ServeFilesFromSourceDirectory("chrome/test/data");
  server_.RegisterRequestHandler(base::BindRepeating(
      &RobotsRulesTestServer::OnServerRequest, base::Unretained(this)));
  server_.RegisterRequestMonitor(base::BindRepeating(
      &RobotsRulesTestServer::OnRequestMonitor, base::Unretained(this)));
  return server_.Start();
}

void RobotsRulesTestServer::AddRobotsRules(
    const GURL& origin,
    const std::vector<RobotsRule>& robots_rules) {
  robots_rules_proto_[origin.spec()] = GetRobotsRulesProtoString(robots_rules);
}

void RobotsRulesTestServer::VerifyRequestedOrigins(
    const std::set<std::string>& requests) {
  EXPECT_EQ(received_requests_, requests);
}

std::unique_ptr<net::test_server::HttpResponse>
RobotsRulesTestServer::OnServerRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  std::string robots_url_str;
  EXPECT_EQ("/robots", request.GetURL().path());
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request.GetURL(), "u", &robots_url_str));
  GURL robots_url(robots_url_str);
  EXPECT_EQ("/robots.txt", GURL(robots_url).path());

  switch (failure_mode_) {
    case FailureMode::kLoadshed503RetryAfterResponse:
      response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
      response->AddCustomHeader("Retry-After", "5");
      return response;
    case FailureMode::kTimeout:
      response = std::make_unique<net::test_server::DelayedHttpResponse>(
          base::TimeDelta::FromSeconds(3));
      break;
    case FailureMode::kNone:
      break;
  }

  auto it = robots_rules_proto_.find(robots_url.GetOrigin().spec());
  if (it != robots_rules_proto_.end())
    response->set_content(it->second);
  return std::move(response);
}

void RobotsRulesTestServer::OnRequestMonitor(
    const net::test_server::HttpRequest& request) {
  std::string robots_url_str;
  EXPECT_EQ("/robots", request.GetURL().path());
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request.GetURL(), "u", &robots_url_str));
  std::string robots_origin = GURL(robots_url_str).GetOrigin().spec();
  EXPECT_TRUE(received_requests_.find(robots_origin) ==
              received_requests_.end());
  received_requests_.insert(robots_origin);
}

ImageCompressionTestServer::ImageCompressionTestServer()
    : server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

ImageCompressionTestServer::~ImageCompressionTestServer() = default;

bool ImageCompressionTestServer::Start() {
  server_.ServeFilesFromSourceDirectory("chrome/test/data");
  server_.RegisterRequestHandler(base::BindRepeating(
      &ImageCompressionTestServer::OnServerRequest, base::Unretained(this)));
  server_.RegisterRequestMonitor(base::BindRepeating(
      &ImageCompressionTestServer::OnRequestMonitor, base::Unretained(this)));
  return server_.Start();
}

void ImageCompressionTestServer::VerifyRequestedImagePaths(
    const std::set<std::string>& paths) {
  EXPECT_EQ(received_request_paths_, paths);
}

std::unique_ptr<net::test_server::HttpResponse>
ImageCompressionTestServer::OnServerRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();

  switch (failure_mode_) {
    case FailureMode::kLoadshed503RetryAfterResponse:
      response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
      response->AddCustomHeader("Retry-After", "5");
      return response;
    case FailureMode::kNone:
      break;
  }

  // Serve the correct image file.
  std::string img_url;
  std::string file_contents;
  base::FilePath test_data_directory;
  EXPECT_EQ("/i", request.GetURL().path());
  EXPECT_TRUE(net::GetValueForKeyInQuery(request.GetURL(), "u", &img_url));
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory);
  if (base::ReadFileToString(
          test_data_directory.AppendASCII(GURL(img_url).path().substr(1)),
          &file_contents)) {
    response->AddCustomHeader("Chrome-Proxy", "ofcl=10000");
    response->set_content(file_contents);
    response->set_code(net::HTTP_OK);
  }
  return std::move(response);
}

// Called on every subresource request
void ImageCompressionTestServer::OnRequestMonitor(
    const net::test_server::HttpRequest& request) {
  std::string img_url;
  EXPECT_EQ("/i", request.GetURL().path());
  EXPECT_TRUE(net::GetValueForKeyInQuery(request.GetURL(), "u", &img_url));
  img_url = GURL(img_url).PathForRequest();
  EXPECT_TRUE(received_request_paths_.find(img_url) ==
              received_request_paths_.end());
  received_request_paths_.insert(img_url);
}

}  // namespace subresource_redirect
