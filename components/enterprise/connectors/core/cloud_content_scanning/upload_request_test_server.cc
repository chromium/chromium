// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/upload_request_test_server.h"

#include <optional>

#include "base/functional/bind.h"
#include "net/http/http_status_code.h"

namespace enterprise_connectors::test {

namespace {

// Safe Browsing Enterprise Upload url.
constexpr char kSbEnterpriseUpload[] = "/safebrowsing/uploads/scan";

}  // namespace

UploadRequestTestServer::UploadRequestTestServer()
    : http_server_(net::test_server::EmbeddedTestServer::TYPE_HTTP) {
  http_server_.RegisterDefaultHandler(base::BindRepeating(
      &UploadRequestTestServer::HandleRequest, base::Unretained(this)));
}

UploadRequestTestServer::~UploadRequestTestServer() = default;

GURL UploadRequestTestServer::GetServiceURL() const {
  return http_server_.GetURL(kSbEnterpriseUpload);
}

void UploadRequestTestServer::SetScanResultSuccess() {
  scan_result_ = TriggeredRule::REPORT_ONLY;
}

void UploadRequestTestServer::SetScanResultWarn() {
  scan_result_ = TriggeredRule::WARN;
}

void UploadRequestTestServer::SetScanResultBlock() {
  scan_result_ = TriggeredRule::BLOCK;
}

bool UploadRequestTestServer::Start() {
  return !!(http_server_handle_ = http_server_.StartAndReturnHandle());
}

std::unique_ptr<net::test_server::HttpResponse>
UploadRequestTestServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (!request.relative_url.starts_with(kSbEnterpriseUpload)) {
    return nullptr;
  }

  // Keep track of the number of scan requests.
  request_counter_++;

  enterprise_connectors::ContentAnalysisResponse analysis_response;

  // Since the Analysis Connectors Prefs contains both DLP and Malware tags, we
  // need to add both of the result.
  auto* dlp_result = analysis_response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* rule = dlp_result->add_triggered_rules();
  rule->set_rule_name("warning_rule_name");
  rule->set_action(scan_result_);

  auto* malware_result = analysis_response.add_results();
  malware_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(scan_result_);

  std::string serialized_analysis_response;
  analysis_response.SerializeToString(&serialized_analysis_response);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(serialized_analysis_response);

  return response;
}

int UploadRequestTestServer::GetRequestCount() const {
  return request_counter_.load();
}

}  // namespace enterprise_connectors::test
