// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_ENVIRONMENT_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_ENVIRONMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace enterprise_connectors::test {

// A fake server environment for testing security event reporting.
class RealtimeReportingTestEnvironment {
 public:
  explicit RealtimeReportingTestEnvironment(
      std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server);
  ~RealtimeReportingTestEnvironment();

  RealtimeReportingTestEnvironment() = delete;
  RealtimeReportingTestEnvironment(const RealtimeReportingTestEnvironment&) =
      delete;
  RealtimeReportingTestEnvironment& operator=(
      const RealtimeReportingTestEnvironment&) = delete;

  // Create a new environment with the given reporting settings. Returns
  // `nullptr` if the server could not be created.
  static std::unique_ptr<RealtimeReportingTestEnvironment> Create(
      const base::flat_set<std::string>& enabled_event_names =
          base::flat_set<std::string>(),
      const base::flat_map<std::string, std::vector<std::string>>&
          enabled_opt_in_events =
              base::flat_map<std::string, std::vector<std::string>>());

  // Bind the server to a port and start listening for requests. Returns true
  // iff the server successfully started.
  bool Start();

  // Get arguments to append to the test process's command line.
  std::vector<std::string> GetArguments();

  // Get the realtime reports uploaded so far.
  std::vector<::chrome::cros::reporting::proto::UploadEventsRequest>
  GetUploadedReports() const;

 private:
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server_;
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_ENVIRONMENT_H_
