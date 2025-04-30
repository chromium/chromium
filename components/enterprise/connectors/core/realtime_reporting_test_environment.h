// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_ENVIRONMENT_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_ENVIRONMENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/enterprise/connectors/core/realtime_reporting_test_server.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

namespace enterprise_connectors::test {

// A set of fake servers for testing security event reporting.
class RealtimeReportingTestEnvironment {
 public:
  RealtimeReportingTestEnvironment(
      std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server,
      std::unique_ptr<RealtimeReportingTestServer> reporting_server);
  ~RealtimeReportingTestEnvironment();

  RealtimeReportingTestEnvironment() = delete;
  RealtimeReportingTestEnvironment(const RealtimeReportingTestEnvironment&) =
      delete;
  RealtimeReportingTestEnvironment& operator=(
      const RealtimeReportingTestEnvironment&) = delete;

  // Create a new environment with the given reporting settings. Returns
  // `nullptr` if any server could not be created.
  static std::unique_ptr<RealtimeReportingTestEnvironment> Create(
      const std::set<std::string>& enabled_event_names =
          std::set<std::string>(),
      const std::map<std::string, std::vector<std::string>>&
          enabled_opt_in_events =
              std::map<std::string, std::vector<std::string>>());

  // Bind each server to a port and start listening for requests. Returns true
  // iff all servers successfully started.
  bool Start();

  // Get arguments to append to the test process's command line.
  std::vector<std::string> GetArguments();

  RealtimeReportingTestServer* reporting_server() const {
    return reporting_server_.get();
  }

 private:
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server_;
  std::unique_ptr<RealtimeReportingTestServer> reporting_server_;
};

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REALTIME_REPORTING_TEST_ENVIRONMENT_H_
