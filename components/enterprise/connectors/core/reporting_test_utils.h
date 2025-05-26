// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"

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

  void ExpectNoReport();

  void ExpectURLFilteringInterstitialEvent(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent event);

  void ExpectURLFilteringInterstitialEventWithReferrers(
      chrome::cros::reporting::proto::UrlFilteringInterstitialEvent event);

  // TODO(crbug.com/396438091): Delete this method once proto migration is
  // complete.
  void ExpectLoginEvent(const std::string& expected_url,
                        const bool expected_is_federated,
                        const std::string& expected_federated_origin,
                        const std::string& expected_profile_username,
                        const std::string& expected_profile_identifier,
                        const std::u16string& expected_login_username);

  void ExpectLoginEvent(
      chrome::cros::reporting::proto::LoginEvent expected_login_event);

  // TODO(crbug.com/396436374): Use password breach event proto instead of raw
  // json string for validation.
  void ExpectPasswordBreachEvent(
      const std::string& expected_trigger,
      const std::vector<std::pair<std::string, std::u16string>>&
          expected_identities,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier);

  // TODO(crbug.com/396437152): Use password reuse event proto instead of raw
  // json string for validation.
  void ExpectPasswordReuseEvent(const std::string& expected_url,
                                const std::string& expected_username,
                                bool expected_is_phishing_url,
                                const std::string& event_result,
                                const std::string& expected_profile_username,
                                const std::string& expected_profile_identifier);

  // TODO(crbug.com/396437063): Use password changed event proto instead of raw
  // json string for validation.
  void ExpectPassowrdChangedEvent(
      const std::string& expected_username,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier);

  // TODO(crbug.com/396437371): Use secutiry interstital event proto instead of
  // raw json string for validation.
  void ExpectSecurityInterstitialEvent(
      const std::string& expected_url,
      const std::string& expected_reason,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::string& result,
      const bool expected_click_through,
      int expected_net_error_code);

  void ExpectSecurityInterstitialEventWithReferrers(
      const std::string& expected_url,
      const std::string& expected_reason,
      const std::string& expected_profile_username,
      const std::string& expected_profile_identifier,
      const std::string& result,
      const bool expected_click_through,
      int expected_net_error_code,
      const ::chrome::cros::reporting::proto::UrlInfo& expected_referrers);

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
                     int expected_value);
  void ValidateField(const base::Value::Dict* value,
                     const std::string& field_key,
                     bool expected_value);
  void ValidateThreatInfo(
      const base::Value::Dict* value,
      const chrome::cros::reporting::proto::TriggeredRuleInfo
          expected_rule_info);
  void ValidateReferrer(
      const base::Value::Dict* value,
      const chrome::cros::reporting::proto::UrlInfo expected_referrer);
  void ValidateFederatedOrigin(const base::Value::Dict* value,
                               const std::string& expected_federated_origin);
  void ValidateIdentities(
      const base::Value::Dict* value,
      const std::vector<std::pair<std::string, std::u16string>>&
          expected_identities);

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
    const std::set<std::string>& enabled_event_names = std::set<std::string>(),
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events =
            std::map<std::string, std::vector<std::string>>());

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_TEST_UTILS_H_
