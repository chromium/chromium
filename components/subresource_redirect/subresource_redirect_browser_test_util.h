// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_BROWSER_TEST_UTIL_H_
#define COMPONENTS_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_BROWSER_TEST_UTIL_H_

#include <map>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "components/subresource_redirect/subresource_redirect_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace subresource_redirect {

// Retries fetching |histogram_name| until it contains at least |count| samples.
void RetryForHistogramUntilCountReached(base::HistogramTester* histogram_tester,
                                        const std::string& histogram_name,
                                        size_t count);

// Fetches histograms from renderer child processes.
void FetchHistogramsFromChildProcesses();

// Embedded test server for the robots rules.
class RobotsRulesTestServer {
 public:
  // Different failures modes the robots server should return.
  enum FailureMode {
    kNone = 0,
    kLoadshed503RetryAfterResponse,
    kTimeout,
  };

  RobotsRulesTestServer();
  ~RobotsRulesTestServer();

  // Start the server.
  bool Start();

  std::string GetURL() const {
    return server_.GetURL("robotsrules.com", "/").spec();
  }

  void AddRobotsRules(const GURL& origin,
                      const std::vector<RobotsRule>& robots_rules);

  void VerifyRequestedOrigins(const std::set<std::string>& requests);

  std::set<std::string> received_requests() const { return received_requests_; }

  void set_failure_mode(FailureMode failure_mode) {
    failure_mode_ = failure_mode;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> OnServerRequest(
      const net::test_server::HttpRequest& request);

  // Called on every robots request.
  void OnRequestMonitor(const net::test_server::HttpRequest& request);

  // Robots rules proto keyed by origin.
  std::map<std::string, std::string> robots_rules_proto_;

  // Whether the robots server should return failure.
  FailureMode failure_mode_ = FailureMode::kNone;

  // All the origins the robots rules are requested for.
  std::set<std::string> received_requests_;

  net::EmbeddedTestServer server_;
};

// Embedded test server to serve the image resources.
class ImageCompressionTestServer {
 public:
  // Different failures modes the image server should return
  enum FailureMode {
    kNone = 0,
    kLoadshed503RetryAfterResponse,
  };
  ImageCompressionTestServer();
  ~ImageCompressionTestServer();

  // Start the server.
  bool Start();

  std::string GetURL() const {
    return server_.GetURL("imagecompression.com", "/").spec();
  }

  void VerifyRequestedImagePaths(const std::set<std::string>& paths);

  void set_failure_mode(FailureMode failure_mode) {
    failure_mode_ = failure_mode;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> OnServerRequest(
      const net::test_server::HttpRequest& request);

  // Called on every subresource request.
  void OnRequestMonitor(const net::test_server::HttpRequest& request);

  // All the URL paths of the requested images.
  std::set<std::string> received_request_paths_;

  // Whether the subresource server should return failure.
  FailureMode failure_mode_ = FailureMode::kNone;

  net::EmbeddedTestServer server_;
};

}  // namespace subresource_redirect

#endif  // COMPONENTS_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_BROWSER_TEST_UTIL_H_
