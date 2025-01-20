// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"

class PrefService;

namespace policy {
class MockCloudPolicyClient;
}

namespace enterprise_connectors::test {

// Helper class that represents a report that's expected from a test. Members
// are protected instead of private to allow sub-classing for specific
// platforms.
class EventReportValidatorBase {
 public:
  explicit EventReportValidatorBase(policy::MockCloudPolicyClient* client);
  ~EventReportValidatorBase();

  void ExpectURLFilteringInterstitialEvent(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent event);

 protected:
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<std::string>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<std::u16string>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<int>& expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     const std::optional<bool>& expected_value);
  void ValidateThreatInfo(
      const base::Value::Dict* value,
      const chrome::cros::reporting::proto::TriggeredRuleInfo
          expected_rule_info);

  raw_ptr<policy::MockCloudPolicyClient> client_;
  base::RepeatingClosure done_closure_;
};

// Helper function to set "OnSecurityEventEnterpriseConnector" for tests.
void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names = std::set<std::string>(),
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events =
            std::map<std::string, std::vector<std::string>>(),
    bool machine_scope = true);

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
