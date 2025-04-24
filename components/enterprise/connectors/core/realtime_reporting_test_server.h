// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_SERVER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_SERVER_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;

namespace net::test_server {
class HttpResponse;
struct HttpRequest;
}  // namespace net::test_server

namespace enterprise_connectors::test {

// Fake realtime reporting server that integration tests can embed.
class RealtimeReportingTestServer {
 public:
  RealtimeReportingTestServer();
  ~RealtimeReportingTestServer();

  // Get a suitable URL to pass to `--realtime-reporting-url`.
  GURL GetServiceURL() const;

  // Bind the server to a port and start listening for requests. Returns true
  // iff the server successfully started.
  bool Start();

  // Get the reports uploaded so far.
  std::vector<::chrome::cros::reporting::proto::UploadEventsRequest>
  GetUploadedReports();

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  // `reports_lock_` guards accesses to `reports_`, which is written on the
  // `http_server_` I/O thread and read on the test thread.
  base::Lock reports_lock_;
  std::vector<::chrome::cros::reporting::proto::UploadEventsRequest> reports_
      GUARDED_BY(reports_lock_);

  net::test_server::EmbeddedTestServer http_server_;
  net::test_server::EmbeddedTestServerHandle http_server_handle_;
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_SERVER_H_
