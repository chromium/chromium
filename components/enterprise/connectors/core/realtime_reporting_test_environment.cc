// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/realtime_reporting_test_environment.h"

#include "base/base_switches.h"
#include "base/check.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/test_support/report_storage.h"

namespace enterprise_connectors::test {

RealtimeReportingTestEnvironment::RealtimeReportingTestEnvironment(
    std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server)
    : policy_server_(std::move(policy_server)) {
  CHECK(policy_server_);
}

RealtimeReportingTestEnvironment::~RealtimeReportingTestEnvironment() = default;

// static
std::unique_ptr<RealtimeReportingTestEnvironment>
RealtimeReportingTestEnvironment::Create(
    const base::flat_set<std::string>& enabled_event_names,
    const base::flat_map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_server =
      CreatePolicyTestServerForSecurityEvents(enabled_event_names,
                                              enabled_opt_in_events);
  if (!policy_server) {
    return nullptr;
  }
  return std::make_unique<RealtimeReportingTestEnvironment>(
      std::move(policy_server));
}

bool RealtimeReportingTestEnvironment::Start() {
  return policy_server_->Start();
}

std::vector<std::string> RealtimeReportingTestEnvironment::GetArguments() {
  return {
#if !BUILDFLAG(IS_CHROMEOS)
      base::StrCat({"--", switches::kEnableChromeBrowserCloudManagement}),
#endif
      base::StrCat({"--", policy::switches::kDeviceManagementUrl, "=",
                    policy_server_->GetServiceURL().spec()}),
      base::StrCat({"--", policy::switches::kRealtimeReportingUrl, "=",
                    policy_server_->GetRealtimeReportingURL().spec()}),
  };
}

std::vector<::chrome::cros::reporting::proto::UploadEventsRequest>
RealtimeReportingTestEnvironment::GetUploadedReports() const {
  return policy_server_->report_storage()->GetRealtimeReports();
}

}  // namespace enterprise_connectors::test
