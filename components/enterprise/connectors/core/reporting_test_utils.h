// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"

class PrefService;

namespace policy {
class EmbeddedPolicyTestServer;
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

  void ExpectNoReport();

  void ExpectUrlFilteringInterstitialEvent(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent
          expected_urlf_event);

  void ExpectLoginEvent(
      chrome::cros::reporting::proto::LoginEvent expected_login_event);

  void ExpectPasswordBreachEvent(
      chrome::cros::reporting::proto::PasswordBreachEvent
          expected_password_breach_event);

  void ExpectPasswordReuseEvent(
      chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent
          expected_password_reuse_event);

  void ExpectPasswordChangedEvent(
      chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
          expected_password_changed_event);

  void ExpectSecurityInterstitialEvent(
      chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent
          expected_interstitial_event);

  void ExpectSensitiveDataEvent(
      chrome::cros::reporting::proto::DlpSensitiveDataEvent
          expected_sensitive_data_event);

  // Closure to run once all expected events are validated.
  void SetDoneClosure(base::RepeatingClosure closure);

 protected:
  void ValidateField(const base::DictValue* value,
                     const std::string& field_key,
                     const std::optional<std::string>& expected_value);
  void ValidateField(const base::DictValue* value,
                     const std::string& field_key,
                     const std::optional<std::u16string>& expected_value);
  void ValidateField(const base::DictValue* value,
                     const std::string& field_key,
                     const std::optional<int>& expected_value);
  void ValidateField(const base::DictValue* value,
                     const std::string& field_key,
                     int expected_value);
  void ValidateField(const base::DictValue* value,
                     const std::string& field_key,
                     bool expected_value);
  void ValidateField(const base::DictValue* value,
                     const std::string& field_key,
                     int64_t expected_value);

  raw_ptr<policy::MockCloudPolicyClient> client_;
  base::RepeatingClosure done_closure_;
};

// Helper function to set "OnSecurityEventEnterpriseConnector" for tests.
void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const base::flat_set<std::string>& enabled_event_names =
        base::flat_set<std::string>(),
    const base::flat_map<std::string, std::vector<std::string>>&
        enabled_opt_in_events =
            base::flat_map<std::string, std::vector<std::string>>(),
    bool machine_scope = true);

// Helper function to create a TriggeredRuleInfo for tests.
::chrome::cros::reporting::proto::TriggeredRuleInfo MakeTriggeredRuleInfo(
    ::chrome::cros::reporting::proto::TriggeredRuleInfo::Action action,
    bool has_watermark);

// Helper function to create a ReferrerChainEntry referrer for tests.
safe_browsing::ReferrerChainEntry MakeReferrerChainEntry();

// Helper function to create a UrlInfo referrer for tests.
::chrome::cros::reporting::proto::UrlInfo MakeUrlInfoReferrer();

// Create a policy server that vends the cloud-only
// "OnSecurityEventEnterpriseConnector" policy for integration tests. Returns
// `nullptr` if the server could not be created.
std::unique_ptr<policy::EmbeddedPolicyTestServer>
CreatePolicyTestServerForSecurityEvents(
    const base::flat_set<std::string>& enabled_event_names =
        base::flat_set<std::string>(),
    const base::flat_map<std::string, std::vector<std::string>>&
        enabled_opt_in_events =
            base::flat_map<std::string, std::vector<std::string>>());

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
