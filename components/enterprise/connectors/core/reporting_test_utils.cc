// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_test_utils.h"

#include <cstddef>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

namespace {

using base::test::EqualsProto;

base::Value::List CreateOptInEventsList(
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  base::Value::List enabled_opt_in_events_list;
  for (const auto& enabled_opt_in_event : enabled_opt_in_events) {
    base::Value::Dict event_value;
    event_value.Set(kKeyOptInEventName, enabled_opt_in_event.first);

    base::Value::List url_patterns_list;
    for (const auto& url_pattern : enabled_opt_in_event.second) {
      url_patterns_list.Append(url_pattern);
    }
    event_value.Set(kKeyOptInEventUrlPatterns, std::move(url_patterns_list));

    enabled_opt_in_events_list.Append(std::move(event_value));
  }
  return enabled_opt_in_events_list;
}

base::Value::Dict CreateSecurityEventReportingSettings(
    const std::set<std::string>& enabled_event_names,
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  base::Value::Dict settings;

  settings.Set(kKeyServiceProvider, base::Value("google"));
  if (!enabled_event_names.empty()) {
    base::Value::List enabled_event_name_list;
    for (const auto& enabled_event_name : enabled_event_names) {
      enabled_event_name_list.Append(enabled_event_name);
    }
    settings.Set(kKeyEnabledEventNames, std::move(enabled_event_name_list));
  }

  if (!enabled_opt_in_events.empty()) {
    settings.Set(kKeyEnabledOptInEvents,
                 CreateOptInEventsList(enabled_opt_in_events));
  }

  return settings;
}

constexpr char kKeyURL[] = "url";
constexpr char kKeyEventResult[] = "eventResult";
constexpr char kKeyTriggeredRuleInfo[] = "triggeredRuleInfo";
constexpr char kKeyTriggeredRuleName[] = "ruleName";
constexpr char kKeyTriggeredRuleId[] = "ruleId";
constexpr char kKeyUrlCategory[] = "urlCategory";
constexpr char kKeyUserName[] = "userName";
constexpr char kKeyAction[] = "action";
constexpr char kKeyHasWatermarking[] = "hasWatermarking";
constexpr char kKeyIsFederated[] = "isFederated";
constexpr char kKeyIsPhishingUrl[] = "isPhishingUrl";
constexpr char kKeyFederatedOrigin[] = "federatedOrigin";
constexpr char kKeyProfileIdentifier[] = "profileIdentifier";
constexpr char kKeyProfileUserName[] = "profileUserName";
constexpr char kKeyLoginUserName[] = "loginUserName";
constexpr char kKeyTrigger[] = "trigger";
constexpr char kKeyPasswordBreachIdentities[] = "identities";
constexpr char kKeyPasswordBreachIdentitiesUsername[] = "username";
constexpr char kKeyPasswordBreachIdentitiesUrl[] = "url";
constexpr char kReferrers[] = "referrers";
constexpr char kKeyIp[] = "ip";

}  // namespace

void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const std::set<std::string>& enabled_event_names,
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events,
    bool machine_scope) {
  ScopedListPrefUpdate settings_list(prefs, kOnSecurityEventPref);
  settings_list->clear();
  prefs->ClearPref(kOnSecurityEventScopePref);
  if (!enabled) {
    return;
  }

  settings_list->Append(CreateSecurityEventReportingSettings(
      enabled_event_names, enabled_opt_in_events));

  prefs->SetInteger(
      kOnSecurityEventScopePref,
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

::chrome::cros::reporting::proto::TriggeredRuleInfo MakeTriggeredRuleInfo(
    ::chrome::cros::reporting::proto::TriggeredRuleInfo::Action action,
    bool has_watermark) {
  ::chrome::cros::reporting::proto::TriggeredRuleInfo info;
  info.set_action(action);
  info.set_rule_id(123);
  info.set_rule_name("test rule name");
  info.set_url_category("test rule category");
  if (has_watermark) {
    info.set_has_watermarking(true);
  }
  return info;
}

safe_browsing::ReferrerChainEntry MakeReferrerChainEntry() {
  safe_browsing::ReferrerChainEntry referrer_chain_entry;
  referrer_chain_entry.set_url("https://referrer.com");
  referrer_chain_entry.set_main_frame_url("https://referrer.com");
  referrer_chain_entry.set_type(safe_browsing::ReferrerChainEntry::EVENT_URL);
  referrer_chain_entry.set_navigation_initiation(
      safe_browsing::ReferrerChainEntry::BROWSER_INITIATED);
  referrer_chain_entry.set_navigation_time_msec(1000);
  referrer_chain_entry.add_ip_addresses("1.2.3.4");
  return referrer_chain_entry;
}

::chrome::cros::reporting::proto::UrlInfo MakeUrlInfoReferrer() {
  ::chrome::cros::reporting::proto::UrlInfo referrers;
  referrers.set_url("https://referrer.com");
  referrers.set_ip("1.2.3.4");
  return referrers;
}
std::unique_ptr<policy::EmbeddedPolicyTestServer>
CreatePolicyTestServerForSecurityEvents(
    const std::set<std::string>& enabled_event_names,
    const std::map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
#if BUILDFLAG(IS_FUCHSIA)
  // Policy is not supported for Fuchsia yet.
  return nullptr;
#else
  base::Value::List reporting_settings =
      base::Value::List().Append(CreateSecurityEventReportingSettings(
          enabled_event_names, enabled_opt_in_events));
  std::optional<std::string> reporting_settings_payload =
      base::WriteJson(reporting_settings);
  if (!reporting_settings_payload) {
    return nullptr;
  }

  enterprise_management::CloudPolicySettings settings;
  settings.mutable_onsecurityevententerpriseconnector()
      ->mutable_policy_options()
      ->set_mode(enterprise_management::PolicyOptions::MANDATORY);
  settings.mutable_onsecurityevententerpriseconnector()->set_value(
      std::move(*reporting_settings_payload));

  auto policy_server = std::make_unique<policy::EmbeddedPolicyTestServer>();
  policy::PolicyStorage* policy_storage = policy_server->policy_storage();
  policy_storage->SetPolicyPayload(
      policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      settings.SerializeAsString());
  return policy_server;
#endif
}

EventReportValidatorBase::EventReportValidatorBase(
    policy::MockCloudPolicyClient* client)
    : client_(client) {}
EventReportValidatorBase::~EventReportValidatorBase() {
  testing::Mock::VerifyAndClearExpectations(client_);
}

void EventReportValidatorBase::ExpectNoReport() {
  if (base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    EXPECT_CALL(*client_, UploadSecurityEvent).Times(0);
  } else {
    EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
  }
}

void EventReportValidatorBase::ExpectURLFilteringInterstitialEvent(
    chrome::cros::reporting::proto::UrlFilteringInterstitialEvent
        expected_urlf_event) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_urlf_event](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event = wrapper.FindDict(
            enterprise_connectors::kKeyUrlFilteringInterstitialEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyURL, expected_urlf_event.url());
        ValidateField(event, kKeyEventResult,
                      chrome::cros::reporting::proto::EventResult_Name(
                          expected_urlf_event.event_result()));
        ValidateField(event, kKeyProfileIdentifier,
                      expected_urlf_event.profile_identifier());
        const base::Value::List* triggered_rules =
            event->FindList(kKeyTriggeredRuleInfo);
        ASSERT_TRUE(triggered_rules);
        ASSERT_EQ(base::checked_cast<size_t>(
                      expected_urlf_event.triggered_rule_info_size()),
                  triggered_rules->size());
        for (size_t i = 0; i < triggered_rules->size(); ++i) {
          const base::Value::Dict& rule = (*triggered_rules)[i].GetDict();
          ValidateThreatInfo(&rule, expected_urlf_event.triggered_rule_info(i));
        }
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidatorBase::ExpectURLFilteringInterstitialEventWithReferrers(
    chrome::cros::reporting::proto::UrlFilteringInterstitialEvent
        expected_urlf_event) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_urlf_event](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event = wrapper.FindDict(
            enterprise_connectors::kKeyUrlFilteringInterstitialEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyURL, expected_urlf_event.url());
        ValidateField(event, kKeyEventResult,
                      chrome::cros::reporting::proto::EventResult_Name(
                          expected_urlf_event.event_result()));
        ValidateField(event, kKeyProfileIdentifier,
                      expected_urlf_event.profile_identifier());
        const base::Value::List* triggered_rules =
            event->FindList(kKeyTriggeredRuleInfo);
        ASSERT_TRUE(triggered_rules);
        ASSERT_EQ(base::checked_cast<size_t>(
                      expected_urlf_event.triggered_rule_info_size()),
                  triggered_rules->size());
        for (size_t i = 0; i < triggered_rules->size(); ++i) {
          const base::Value::Dict& rule = (*triggered_rules)[i].GetDict();
          ValidateThreatInfo(&rule, expected_urlf_event.triggered_rule_info(i));
        }
        const base::Value::List* referrers = event->FindList(kReferrers);
        ASSERT_TRUE(referrers);
        for (size_t i = 0; i < referrers->size(); ++i) {
          const base::Value::Dict& referrer = (*referrers)[i].GetDict();
          ValidateReferrer(&referrer, expected_urlf_event.referrers(i));
        }
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidatorBase::ExpectLoginEvent(
    const std::string& expected_url,
    const bool expected_is_federated,
    const std::string& expected_federated_origin,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::u16string& expected_login_username) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_url, expected_is_federated,
                 expected_federated_origin, expected_profile_username,
                 expected_profile_identifier, expected_login_username](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event =
            wrapper.FindDict(enterprise_connectors::kKeyLoginEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyURL, expected_url);
        ValidateField(event, kKeyIsFederated, expected_is_federated);
        ValidateFederatedOrigin(event, expected_federated_origin);
        ValidateField(event, kKeyProfileUserName, expected_profile_username);
        ValidateField(event, kKeyProfileIdentifier,
                      expected_profile_identifier);
        ValidateField(event, kKeyLoginUserName, expected_login_username);

        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidatorBase::ExpectLoginEvent(
    chrome::cros::reporting::proto::LoginEvent expected_login_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_login_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                  callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_login_event());
            auto login_event = request.events().Get(0).login_event();
            EXPECT_THAT(login_event, EqualsProto(expected_login_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::ExpectPasswordBreachEvent(
    const std::string& expected_trigger,
    const std::vector<std::pair<std::string, std::u16string>>&
        expected_identities,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_trigger, expected_identities,
                 expected_profile_username, expected_profile_identifier](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event =
            wrapper.FindDict(enterprise_connectors::kKeyPasswordBreachEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyTrigger, expected_trigger);
        ValidateIdentities(event, std::move(expected_identities));
        ValidateField(event, kKeyProfileUserName, expected_profile_username);
        ValidateField(event, kKeyProfileIdentifier,
                      expected_profile_identifier);
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidatorBase::ExpectPasswordReuseEvent(
    const std::string& expected_url,
    const std::string& expected_username,
    bool expected_is_phishing_url,
    const std::string& event_result,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_url, expected_username,
                 expected_is_phishing_url, event_result,
                 expected_profile_username, expected_profile_identifier](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event =
            wrapper.FindDict(enterprise_connectors::kKeyPasswordReuseEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyURL, expected_url);
        ValidateField(event, kKeyUserName, expected_username);
        ValidateField(event, kKeyIsPhishingUrl, expected_is_phishing_url);
        ValidateField(event, kKeyEventResult, event_result);
        ValidateField(event, kKeyProfileUserName, expected_profile_username);
        ValidateField(event, kKeyProfileIdentifier,
                      expected_profile_identifier);
      });
}

void EventReportValidatorBase::ExpectPassowrdChangedEvent(
    const std::string& expected_username,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_username, expected_profile_username,
                 expected_profile_identifier](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event =
            wrapper.FindDict(enterprise_connectors::kKeyPasswordChangedEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyUserName, expected_username);
        ValidateField(event, kKeyProfileUserName, expected_profile_username);
        ValidateField(event, kKeyProfileIdentifier,
                      expected_profile_identifier);
      });
}

void EventReportValidatorBase::ExpectSecurityInterstitialEvent(
    const std::string& expected_url,
    const std::string& expected_reason,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::string& result,
    const bool expected_click_through,
    int expected_net_error_code) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_url, expected_reason, expected_profile_username,
                 expected_profile_identifier, result, expected_click_through,
                 expected_net_error_code](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event =
            wrapper.FindDict(enterprise_connectors::kKeyInterstitialEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyURL, expected_url);
        ValidateField(event, kKeyReason, expected_reason);
        ValidateField(event, kKeyNetErrorCode, expected_net_error_code);
        ValidateField(event, kKeyClickedThrough, expected_click_through);
        ValidateField(event, kKeyProfileUserName, expected_profile_username);
        ValidateField(event, kKeyProfileIdentifier,
                      expected_profile_identifier);
        ValidateField(event, kKeyEventResult, result);
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidatorBase::ExpectSecurityInterstitialEventWithReferrers(
    const std::string& expected_url,
    const std::string& expected_reason,
    const std::string& expected_profile_username,
    const std::string& expected_profile_identifier,
    const std::string& result,
    const bool expected_click_through,
    int expected_net_error_code,
    const ::chrome::cros::reporting::proto::UrlInfo& expected_referrers) {
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce([this, expected_url, expected_reason, expected_profile_username,
                 expected_profile_identifier, result, expected_click_through,
                 expected_net_error_code, expected_referrers](
                    bool include_device_info, base::Value::Dict report,
                    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                        callback) {
        // Extract the event list.
        const base::Value::List* event_list = report.FindList(
            policy::RealtimeReportingJobConfiguration::kEventListKey);
        ASSERT_TRUE(event_list);

        // There should only be 1 event per test.
        ASSERT_EQ(1u, event_list->size());
        const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
        const base::Value::Dict* event =
            wrapper.FindDict(enterprise_connectors::kKeyInterstitialEvent);
        ASSERT_TRUE(event);

        ValidateField(event, kKeyURL, expected_url);
        ValidateField(event, kKeyReason, expected_reason);
        ValidateField(event, kKeyNetErrorCode, expected_net_error_code);
        ValidateField(event, kKeyClickedThrough, expected_click_through);
        ValidateField(event, kKeyProfileUserName, expected_profile_username);
        ValidateField(event, kKeyProfileIdentifier,
                      expected_profile_identifier);
        ValidateField(event, kKeyEventResult, result);
        const base::Value::List* referrers = event->FindList(kReferrers);
        ASSERT_TRUE(referrers);
        for (const auto & referrer : *referrers) {
          ValidateReferrer(&referrer.GetDict(), expected_referrers);
        }
        if (!done_closure_.is_null()) {
          done_closure_.Run();
        }
      });
}

void EventReportValidatorBase::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const std::optional<std::string>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_EQ(*value->FindString(field_key), expected_value.value())
        << "Mismatch in field " << field_key
        << "\nActual value: " << value->FindString(field_key)
        << "\nExpected value: " << expected_value.value();
  } else {
    ASSERT_EQ(nullptr, value->FindString(field_key))
        << "Field " << field_key << " should not be populated. It has value "
        << *value->FindString(field_key);
  }
}

void EventReportValidatorBase::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const std::optional<std::u16string>& expected_value) {
  const std::string* s = value->FindString(field_key);
  if (expected_value.has_value()) {
    const std::u16string actual_string_value = base::UTF8ToUTF16(*s);
    ASSERT_EQ(actual_string_value, expected_value.value())
        << "Mismatch in field " << field_key
        << "\nActual value: " << actual_string_value
        << "\nExpected value: " << expected_value.value();
  } else {
    ASSERT_EQ(nullptr, s) << "Field " << field_key
                          << " should not be populated. It has value "
                          << *value->FindString(field_key);
  }
}

void EventReportValidatorBase::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const std::optional<int>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_EQ(value->FindInt(field_key), expected_value)
        << "Mismatch in field " << field_key
        << "\nActual value: " << value->FindInt(field_key).value()
        << "\nExpected value: " << expected_value.value();
  } else {
    ASSERT_FALSE(value->FindInt(field_key).has_value())
        << "Field " << field_key << " should not be populated. It has value "
        << *value->FindInt(field_key);
  }
}

void EventReportValidatorBase::ValidateField(const base::Value::Dict* value,
                                             const std::string& field_key,
                                             int expected_value) {
  ASSERT_EQ(value->FindInt(field_key), expected_value)
      << "Mismatch in field " << field_key
      << "\nActual value: " << value->FindInt(field_key).value()
      << "\nExpected value: " << expected_value;
}

void EventReportValidatorBase::ValidateField(const base::Value::Dict* value,
                                             const std::string& field_key,
                                             bool expected_value) {
  ASSERT_EQ(value->FindBool(field_key), expected_value)
      << "Mismatch in field " << field_key
      << "\nActual value: " << value->FindBool(field_key).value()
      << "\nExpected value: " << expected_value;
}

void EventReportValidatorBase::ValidateThreatInfo(
    const base::Value::Dict* value,
    const chrome::cros::reporting::proto::TriggeredRuleInfo
        expected_rule_info) {
  ValidateField(value, kKeyTriggeredRuleName, expected_rule_info.rule_name());
  ValidateField(value, kKeyTriggeredRuleId,
                std::optional<int>(expected_rule_info.rule_id()));
  ValidateField(value, kKeyUrlCategory, expected_rule_info.url_category());
  ValidateField(value, kKeyAction,
                chrome::cros::reporting::proto::TriggeredRuleInfo::Action_Name(
                    expected_rule_info.action()));
  if (expected_rule_info.has_watermarking()) {
    ValidateField(value, kKeyHasWatermarking, true);
  }
}

void EventReportValidatorBase::ValidateReferrer(
    const base::Value::Dict* value,
    const chrome::cros::reporting::proto::UrlInfo expected_referrer) {
  ValidateField(value, kKeyURL, expected_referrer.url());
  ValidateField(value, kKeyIp, expected_referrer.ip());
}

void EventReportValidatorBase::ValidateFederatedOrigin(
    const base::Value::Dict* value,
    const std::string& expected_federated_origin) {
  std::optional<bool> is_federated = value->FindBool(kKeyIsFederated);
  const std::string* federated_origin = value->FindString(kKeyFederatedOrigin);
  if (is_federated.has_value() && is_federated.value()) {
    EXPECT_NE(nullptr, federated_origin);
    EXPECT_EQ(expected_federated_origin, *federated_origin);
  } else {
    EXPECT_EQ(nullptr, federated_origin);
  }
}

void EventReportValidatorBase::ValidateIdentities(
    const base::Value::Dict* value,
    const std::vector<std::pair<std::string, std::u16string>>&
        expected_identities) {
  const base::Value::List* identities =
      value->FindList(kKeyPasswordBreachIdentities);
  EXPECT_NE(nullptr, identities);
  EXPECT_EQ(expected_identities.size(), identities->size());
  for (const auto& expected_identity : expected_identities) {
    bool matched = false;
    for (const auto& actual_identity : *identities) {
      const base::Value::Dict& actual_identity_dict = actual_identity.GetDict();
      const std::string* url =
          actual_identity_dict.FindString(kKeyPasswordBreachIdentitiesUrl);
      const std::string* actual_username =
          actual_identity_dict.FindString(kKeyPasswordBreachIdentitiesUsername);
      EXPECT_NE(nullptr, actual_username);
      const std::u16string username = base::UTF8ToUTF16(*actual_username);
      EXPECT_NE(nullptr, url);
      if (expected_identity.first == *url &&
          expected_identity.second == username) {
        matched = true;
        break;
      }
    }
    EXPECT_TRUE(matched);
  }
}

}  // namespace enterprise_connectors::test
