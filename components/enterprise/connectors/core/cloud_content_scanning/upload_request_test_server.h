// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_UPLOAD_REQUEST_TEST_SERVER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_UPLOAD_REQUEST_TEST_SERVER_H_

#include "components/enterprise/common/proto/connectors.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;

namespace net::test_server {
class HttpResponse;
struct HttpRequest;
}  // namespace net::test_server

namespace enterprise_connectors::test {

using TriggeredRule =
    enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule;

// Fake server that accepts the upload scan request and create response for
// Enterprise testing.
class UploadRequestTestServer {
 public:
  UploadRequestTestServer();
  ~UploadRequestTestServer();

  // Get a suitable URL to pass to `--binary-upload-service-url`.
  GURL GetServiceURL() const;

  // Set the Scan result to be REPORT_ONLY, WARN or BLOCK.
  void SetScanResultSuccess();
  void SetScanResultWarn();
  void SetScanResultBlock();

  // Get the number of scan requests that this server received.
  int GetRequestCount() const;

  // Bind the server to a port and start listening for requests. Returns true
  // if the server successfully started.
  bool Start();

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  std::atomic<int> request_counter_ = 0;

  TriggeredRule::Action scan_result_ = TriggeredRule::ACTION_UNSPECIFIED;
  net::test_server::EmbeddedTestServer http_server_;
  net::test_server::EmbeddedTestServerHandle http_server_handle_;
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_UPLOAD_REQUEST_TEST_SERVER_H_
