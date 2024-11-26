// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

namespace {

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

constexpr char kKeyURL[] = "url";
constexpr char kKeyEventResult[] = "eventResult";
constexpr char kKeyTriggeredRuleInfo[] = "triggeredRuleInfo";
constexpr char kKeyTriggeredRuleName[] = "ruleName";
constexpr char kKeyTriggeredRuleId[] = "ruleId";
constexpr char kKeyUrlCategory[] = "urlCategory";
constexpr char kKeyAction[] = "action";
constexpr char kKeyHasWatermarking[] = "hasWatermarking";

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

  settings_list->Append(std::move(settings));

  prefs->SetInteger(
      kOnSecurityEventScopePref,
      machine_scope ? policy::POLICY_SCOPE_MACHINE : policy::POLICY_SCOPE_USER);
}

EventReportValidatorBase::EventReportValidatorBase(
    policy::MockCloudPolicyClient* client)
    : client_(client) {}
EventReportValidatorBase::~EventReportValidatorBase() {
  testing::Mock::VerifyAndClearExpectations(client_);
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
        ValidateField(event,
                      enterprise_connectors::RealtimeReportingClientBase::
                          kKeyProfileIdentifier,
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
  ASSERT_EQ(value->FindInt(field_key), expected_value)
      << "Mismatch in field " << field_key
      << "\nActual value: " << value->FindInt(field_key).value()
      << "\nExpected value: " << expected_value.value();
}

void EventReportValidatorBase::ValidateField(
    const base::Value::Dict* value,
    const std::string& field_key,
    const std::optional<bool>& expected_value) {
  ASSERT_EQ(value->FindBool(field_key), expected_value)
      << "Mismatch in field " << field_key
      << "\nActual value: " << value->FindBool(field_key).value()
      << "\nExpected value: " << expected_value.value();
}

void EventReportValidatorBase::ValidateThreatInfo(
    const base::Value::Dict* value,
    const chrome::cros::reporting::proto::TriggeredRuleInfo
        expected_rule_info) {
  ValidateField(value, kKeyTriggeredRuleName, expected_rule_info.rule_name());
  ValidateField(value, kKeyTriggeredRuleId,
                base::NumberToString(expected_rule_info.rule_id()));
  ValidateField(value, kKeyUrlCategory, expected_rule_info.url_category());
  ValidateField(value, kKeyAction,
                chrome::cros::reporting::proto::TriggeredRuleInfo::Action_Name(
                    expected_rule_info.action()));
  if (expected_rule_info.has_watermarking()) {
    ValidateField(value, kKeyHasWatermarking, std::optional<bool>(true));
  }
}

}  // namespace enterprise_connectors::test
