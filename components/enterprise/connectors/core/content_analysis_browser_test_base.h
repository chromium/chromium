// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_BROWSER_TEST_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_BROWSER_TEST_BASE_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/content_analysis_data.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace enterprise_connectors::test {

// Base class that allows tests to set up an embedded test server that expects
// to receive content analysis requests and that returns responses.
// TODO(crbug.com/488379628): Enhance this class to handle more content analysis
// actions and management contexts.
class ContentAnalysisBrowserTestBase {
 public:
  explicit ContentAnalysisBrowserTestBase(
      net::EmbeddedTestServer* embedded_test_server);
  ~ContentAnalysisBrowserTestBase();

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  // Adds a request-body pair to `expected_requests_` that is expected to be
  // sent to the embedded test server by the test before it completes.
  void AddExpectedScanningRequest(const ContentAnalysisData& data,
                                  const std::string& body);

 private:
  // Returns true if `received_request` matches `expected_request`.
  bool MatchesRequest(ContentAnalysisRequest received_request,
                      ContentAnalysisRequest expected_request);

  // Returns the value to be set in the ConnectorAnalysisRequest::device_token
  // field.
  std::string ExpectedDeviceToken();

  // Helpers called by `HandleRequest()` to validate different kinds of content
  // scanning requests received by the embedded test server.
  std::unique_ptr<net::test_server::HttpResponse>
  HandleResumableMetadataRequest(const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::HttpResponse> HandleResumableContentRequest(
      const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::HttpResponse> HandleMultipartRequest(
      const net::test_server::HttpRequest& request);

  // Helper that adds authorization requests to `expected_requests_` depending
  // on `data` and whether or not an authorization request for that request type
  // already exists.
  void AddAuthRequestIfNeeded(const ContentAnalysisData& data,
                              ContentAnalysisRequest request);

  // Returns a Resumable metadata response indicating the action is not allowed
  // yet and that the content should be sent.
  std::unique_ptr<net::test_server::HttpResponse> SendContentMetadataResponse();

  raw_ptr<net::EmbeddedTestServer> embedded_test_server_;
  std::vector<std::pair<ContentAnalysisRequest, std::string>>
      expected_requests_;

  // Track whether `AddExpectedScanningRequest()` has been called or not for
  // each different type of auth request.
  bool paste_auth_request_added_ = false;
  bool file_attach_auth_request_added_ = false;
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_BROWSER_TEST_BASE_H_
