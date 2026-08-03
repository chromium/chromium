// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_test_utils.h"

#include <cstddef>

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
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
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

namespace {

using base::test::EqualsProto;

base::ListValue CreateOptInEventsList(
    const base::flat_map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  base::ListValue enabled_opt_in_events_list;
  for (const auto& enabled_opt_in_event : enabled_opt_in_events) {
    base::DictValue event_value;
    event_value.Set(kKeyOptInEventName, enabled_opt_in_event.first);

    base::ListValue url_patterns_list;
    for (const auto& url_pattern : enabled_opt_in_event.second) {
      url_patterns_list.Append(url_pattern);
    }
    event_value.Set(kKeyOptInEventUrlPatterns, std::move(url_patterns_list));

    enabled_opt_in_events_list.Append(std::move(event_value));
  }
  return enabled_opt_in_events_list;
}

base::DictValue CreateSecurityEventReportingSettings(
    const base::flat_set<std::string>& enabled_event_names,
    const base::flat_map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
  base::DictValue settings;

  settings.Set(kKeyServiceProvider, base::Value("google"));
  if (!enabled_event_names.empty()) {
    base::ListValue enabled_event_name_list;
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

}  // namespace

void SetOnSecurityEventReporting(
    PrefService* prefs,
    bool enabled,
    const base::flat_set<std::string>& enabled_event_names,
    const base::flat_map<std::string, std::vector<std::string>>&
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
    const base::flat_set<std::string>& enabled_event_names,
    const base::flat_map<std::string, std::vector<std::string>>&
        enabled_opt_in_events) {
#if BUILDFLAG(IS_FUCHSIA)
  // Policy is not supported for Fuchsia yet.
  return nullptr;
#else
  base::ListValue reporting_settings =
      base::ListValue().Append(CreateSecurityEventReportingSettings(
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
  EXPECT_CALL(*client_, UploadSecurityEvent).Times(0);
}

void EventReportValidatorBase::ExpectUrlFilteringInterstitialEvent(
    chrome::cros::reporting::proto::UrlFilteringInterstitialEvent
        expected_urlf_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_urlf_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(
                request.events().Get(0).has_url_filtering_interstitial_event());
            auto urlf_event =
                request.events().Get(0).url_filtering_interstitial_event();
            EXPECT_THAT(urlf_event, EqualsProto(expected_urlf_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::SetDoneClosure(base::RepeatingClosure closure) {
  done_closure_ = std::move(closure);
}

void EventReportValidatorBase::ExpectLoginEvent(
    chrome::cros::reporting::proto::LoginEvent expected_login_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_login_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
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

void EventReportValidatorBase::ExpectSecurityInterstitialEvent(
    chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent
        expected_interstitial_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_interstitial_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_interstitial_event());
            auto interstitial_event =
                request.events().Get(0).interstitial_event();
            EXPECT_THAT(interstitial_event,
                        EqualsProto(expected_interstitial_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::ExpectPasswordBreachEvent(
    chrome::cros::reporting::proto::PasswordBreachEvent
        expected_password_breach_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_password_breach_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_password_breach_event());
            auto password_breach_event =
                request.events().Get(0).password_breach_event();
            EXPECT_THAT(password_breach_event,
                        EqualsProto(expected_password_breach_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::ExpectPasswordChangedEvent(
    chrome::cros::reporting::proto::SafeBrowsingPasswordChangedEvent
        expected_password_changed_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_password_changed_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_password_changed_event());
            auto password_changed_event =
                request.events().Get(0).password_changed_event();
            EXPECT_THAT(password_changed_event,
                        EqualsProto(expected_password_changed_event));
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::ExpectPasswordReuseEvent(
    chrome::cros::reporting::proto::SafeBrowsingPasswordReuseEvent
        expected_password_reuse_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_password_reuse_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_password_reuse_event());
            auto password_reuse_event =
                request.events().Get(0).password_reuse_event();
            EXPECT_THAT(password_reuse_event,
                        EqualsProto(expected_password_reuse_event));
            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::ExpectSensitiveDataEvent(
    chrome::cros::reporting::proto::DlpSensitiveDataEvent
        expected_sensitive_data_event) {
  EXPECT_CALL(*client_, UploadSecurityEvent)
      .WillOnce(
          [this, expected_sensitive_data_event](
              bool include_device_info,
              ::chrome::cros::reporting::proto::UploadEventsRequest request,
              policy::CloudPolicyClient::ResultCallback callback) {
            // There should only be 1 event per test.
            ASSERT_EQ(1, request.events_size());
            ASSERT_TRUE(request.events().Get(0).has_sensitive_data_event());
            auto sensitive_data_event =
                request.events().Get(0).sensitive_data_event();
            EXPECT_THAT(sensitive_data_event,
                        EqualsProto(expected_sensitive_data_event));

            if (!done_closure_.is_null()) {
              done_closure_.Run();
            }
          });
}

void EventReportValidatorBase::ValidateField(
    const base::DictValue* value,
    const std::string& field_key,
    const std::optional<std::string>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_TRUE(value->FindString(field_key))
        << "Mismatch in field " << field_key << "\nNo value was set"
        << "\nExpected value: " << expected_value.value();
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
    const base::DictValue* value,
    const std::string& field_key,
    const std::optional<std::u16string>& expected_value) {
  const std::string* s = value->FindString(field_key);
  if (expected_value.has_value()) {
    ASSERT_TRUE(s) << "Mismatch in field " << field_key << "\nNo value was set"
                   << "\nExpected value: " << expected_value.value();

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
    const base::DictValue* value,
    const std::string& field_key,
    const std::optional<int>& expected_value) {
  if (expected_value.has_value()) {
    ASSERT_TRUE(value->FindInt(field_key).has_value())
        << "Mismatch in field " << field_key << "\nNo value was set"
        << "\nExpected value: " << expected_value.value();
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

void EventReportValidatorBase::ValidateField(const base::DictValue* value,
                                             const std::string& field_key,
                                             int expected_value) {
  ASSERT_TRUE(value->FindInt(field_key).has_value())
      << "Mismatch in field " << field_key << "\nNo value was set"
      << "\nExpected value: " << expected_value;
  ASSERT_EQ(value->FindInt(field_key), expected_value)
      << "Mismatch in field " << field_key
      << "\nActual value: " << value->FindInt(field_key).value()
      << "\nExpected value: " << expected_value;
}

void EventReportValidatorBase::ValidateField(const base::DictValue* value,
                                             const std::string& field_key,
                                             bool expected_value) {
  ASSERT_TRUE(value->FindBool(field_key).has_value())
      << "Mismatch in field " << field_key << "\nNo value was set"
      << "\nExpected value: " << expected_value;
  ASSERT_EQ(value->FindBool(field_key), expected_value)
      << "Mismatch in field " << field_key
      << "\nActual value: " << value->FindBool(field_key).value()
      << "\nExpected value: " << expected_value;
}

void EventReportValidatorBase::ValidateField(const base::DictValue* value,
                                             const std::string& field_key,
                                             int64_t expected_value) {
  ASSERT_TRUE(base::ValueToInt64(value->Find(field_key)).has_value())
      << "Mismatch in field " << field_key << "\nNo value was set"
      << "\nExpected value: " << expected_value;
  ASSERT_EQ(base::ValueToInt64(value->Find(field_key)).value(), expected_value)
      << "Mismatch in field " << field_key << "\nActual value: "
      << base::ValueToInt64(value->Find(field_key)).value()
      << "\nExpected value: " << expected_value;
}

}  // namespace enterprise_connectors::test
