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

  void ExpectScanningRequest(const ContentAnalysisData& data,
                             const std::string& body);

 private:
  // Returns true if `received_request` matches `expected_request`.
  bool MatchesRequest(ContentAnalysisRequest received_request,
                      ContentAnalysisRequest expected_request);

  // Returns the value to be set in the ConnectorAnalysisRequest::device_token field.
  std::string ExpectedDeviceToken();

  raw_ptr<net::EmbeddedTestServer> embedded_test_server_;
  std::vector<std::pair<ContentAnalysisRequest, std::string>>
      expected_requests_;
  bool paste_auth_request_added_ = false;
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_BROWSER_TEST_BASE_H_
