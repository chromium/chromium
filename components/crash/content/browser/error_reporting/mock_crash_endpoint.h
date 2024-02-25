// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_MOCK_CRASH_ENDPOINT_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_MOCK_CRASH_ENDPOINT_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

// Installs a mock crash server endpoint.
class MockCrashEndpoint {
 public:
  struct Report {
    Report(std::string query_value, std::string content_value);
    std::string query;
    std::string content;
  };

  explicit MockCrashEndpoint(net::test_server::EmbeddedTestServer* test_server);
  MockCrashEndpoint(const MockCrashEndpoint&) = delete;
  MockCrashEndpoint& operator=(const MockCrashEndpoint&) = delete;
  ~MockCrashEndpoint();

  // Returns the last received report, or waits for a query and returns it.
  Report WaitForReport();

  // Returns the last report received, if any.
  const std::optional<Report>& last_report() const { return last_report_; }

  // Clears last report so that WaitForReport will wait for another report.
  // Does not clear report_count() or all_reports().
  void clear_last_report() { last_report_.reset(); }

  // Get the number of reports received since this object was created.
  int report_count() const { return report_count_; }

  // Retrieves all the reports received by this mock crash endpoint.
  const std::vector<Report>& all_reports() const { return all_reports_; }

  // Configures whether the mock crash reporter client has user-consent for
  // submitting crash reports.
  void set_consented(bool consented) { consented_ = consented; }

  // Set the response that the server will return.
  void set_response(net::HttpStatusCode code, const std::string& content) {
    response_code_ = code;
    response_content_ = content;
  }

  // Returns the URL that tests should send crash reports to.
  std::string GetCrashEndpointURL() const;

 private:
  class Client;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  raw_ptr<net::test_server::EmbeddedTestServer> test_server_;
  std::unique_ptr<Client> client_;
  std::optional<Report> last_report_;
  std::vector<Report> all_reports_;
  int report_count_ = 0;
  bool consented_ = true;
  base::RepeatingClosure on_report_;
  net::HttpStatusCode response_code_ = net::HTTP_OK;
  std::string response_content_ = "123";
};

std::ostream& operator<<(std::ostream& out,
                         const MockCrashEndpoint::Report& report);

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_MOCK_CRASH_ENDPOINT_H_
