// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_test_util.h"

#include <tuple>
#include <variant>

#include "base/feature_list.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"
namespace privacy_sandbox_test_util {

namespace {

// Convenience function that unpacks a map keyed on both MultipleXKeys, and
// single keys (e.g. keyed on the TestKey variant type), into a map key _only_
// on single keys.
template <typename T>
std::map<T, TestCaseItemValue> UnpackKeys(
    const std::map<TestKey<T>, TestCaseItemValue>& test_key_to_test_value) {
  std::map<T, TestCaseItemValue> unpacked_map;

  for (const auto& [test_key, value] : test_key_to_test_value) {
    // If test_key is a single key, set the value in the map directly.
    if (std::holds_alternative<T>(test_key)) {
      auto key = std::get<T>(test_key);
      EXPECT_EQ(0u, unpacked_map.count(key))
          << "Duplicate test key " << static_cast<int>(key);
      unpacked_map[key] = value;
    } else {
      auto keys = std::get<MultipleKeys<T>>(test_key);
      for (auto key : keys) {
        EXPECT_EQ(0u, unpacked_map.count(key))
            << "Duplicate test key " << static_cast<int>(key);
        unpacked_map[key] = value;
      }
    }
  }

  return unpacked_map;
}

template <typename T>
T GetItemValue(const TestCaseItemValue& value) {
  EXPECT_TRUE(std::holds_alternative<T>(value));
  return std::get<T>(value);
}

template <typename V, typename K>
V GetItemValueForKey(K key, std::map<K, TestCaseItemValue> test_components) {
  EXPECT_TRUE(test_components.count(key))
      << "Unable to find key " << static_cast<int>(key);
  return GetItemValue<V>(test_components.at(key));
}

}  // namespace

void ApplyTestState(
    StateKey key,
    const TestCaseItemValue& value,
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider) {
  switch (key) {
    case (StateKey::kM1TopicsEnabledUserPrefValue): {
      SCOPED_TRACE("State Setup: User M1 Topics pref");
      testing_pref_service->SetUserPref(prefs::kPrivacySandboxM1TopicsEnabled,
                                        base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1FledgeEnabledUserPrefValue): {
      SCOPED_TRACE("State Setup: User M1 Fledge pref");
      testing_pref_service->SetUserPref(prefs::kPrivacySandboxM1FledgeEnabled,
                                        base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1AdMeasurementEnabledUserPrefValue): {
      SCOPED_TRACE("State Setup: User M1 Ad measurement pref");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxM1AdMeasurementEnabled,
          base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kCookieControlsModeUserPrefValue): {
      SCOPED_TRACE("State Setup: User cookies controls mode");

      testing_pref_service->SetUserPref(
          prefs::kCookieControlsMode,
          base::Value(static_cast<int>(
              GetItemValue<content_settings::CookieControlsMode>(value))));
      return;
    }
    case (StateKey::kSiteDataUserDefault): {
      SCOPED_TRACE("State Setup: User site data default");
      auto content_setting = GetItemValue<ContentSetting>(value);

      user_content_setting_provider->SetWebsiteSetting(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
          base::Value(content_setting), /*constraints=*/{});
      return;
    }
    case (StateKey::kSiteDataUserExceptions): {
      SCOPED_TRACE("State Setup: User site data exceptions");
      auto exceptions = GetItemValue<SiteDataExceptions>(value);

      for (const auto& [primary_pattern, content_setting] : exceptions) {
        user_content_setting_provider->SetWebsiteSetting(
            ContentSettingsPattern::FromString(primary_pattern),
            ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
            base::Value(content_setting), /*constraints=*/{});
      }
      return;
    }
    case (StateKey::kIsIncognito): {
      SCOPED_TRACE("State Setup: User Incognito");
      return;
    }
    case (StateKey::kIsRestrictedAccount): {
      SCOPED_TRACE("State Setup: User restricted");
      return;
    }
    case (StateKey::kAdvanceClockBy): {
      auto time_delta = GetItemValue<base::TimeDelta>(value);
      task_environment->AdvanceClock(time_delta);
      return;
    }

    case (StateKey::kM1TopicsDisabledByPolicy): {
      SCOPED_TRACE("State Setup: M1 topics disabled by policy");
      testing_pref_service->SetManagedPref(
          prefs::kPrivacySandboxM1TopicsEnabled, base::Value(false));
      EXPECT_TRUE(testing_pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1TopicsEnabled));
      return;
    }
    case (StateKey::kM1FledgeDisabledByPolicy): {
      SCOPED_TRACE("State Setup: M1 fledge disabled by policy");
      testing_pref_service->SetManagedPref(
          prefs::kPrivacySandboxM1FledgeEnabled, base::Value(false));
      EXPECT_TRUE(testing_pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1FledgeEnabled));
      return;
    }
    case (StateKey::kM1AdMesaurementDisabledByPolicy): {
      SCOPED_TRACE("State Setup: M1 ad measurement disabled by policy");
      testing_pref_service->SetManagedPref(
          prefs::kPrivacySandboxM1AdMeasurementEnabled, base::Value(false));
      EXPECT_TRUE(testing_pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1AdMeasurementEnabled));
      return;
    }
    case (StateKey::kAttestationsMap): {
      SCOPED_TRACE("State Setup: Attestations Map");
      privacy_sandbox::PrivacySandboxAttestations::GetInstance()
          ->SetAttestationsForTesting(
              GetItemValue<std::optional<
                  privacy_sandbox::PrivacySandboxAttestationsMap>>(value));
      return;
    }
    default:
      NOTREACHED();
  }
}

void ProvideInput(const std::pair<InputKey, TestCaseItemValue>& input,
                  PrivacySandboxServiceTestInterface* privacy_sandbox_service) {
  auto [input_key, input_value] = input;
  switch (input_key) {
    case (InputKey::kPromptAction): {
      // OutputKey::kPromptAction is not used.
      // TODO(crbug.com/474716334): Remove this case when the enum is removed.
      return;
    }
    default: {
      return;
    }
  }
}

void CheckOutput(
    const std::map<InputKey, TestCaseItemValue>& input,
    const std::pair<OutputKey, TestCaseItemValue>& output,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service) {
  auto [output_key, output_value] = output;
  switch (output_key) {
    case (OutputKey::kPromptType): {
      // OutputKey::kPromptType is not used.
      // TODO(crbug.com/474716334): Remove this case when the enum is removed.
      return;
    }

    case (OutputKey::kM1TopicsEnabled): {
      SCOPED_TRACE("Check Output: M1 topics enabled");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected, testing_pref_service->GetBoolean(
                              prefs::kPrivacySandboxM1TopicsEnabled));
      return;
    }
    case (OutputKey::kM1FledgeEnabled): {
      SCOPED_TRACE("Check Output: M1 fledge enabled");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected, testing_pref_service->GetBoolean(
                              prefs::kPrivacySandboxM1FledgeEnabled));
      return;
    }
    case (OutputKey::kM1AdMeasurementEnabled): {
      SCOPED_TRACE("Check Output: M1 ad measurement enabled");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected, testing_pref_service->GetBoolean(
                              prefs::kPrivacySandboxM1AdMeasurementEnabled));
      return;
    }
  }
}

MockPrivacySandboxObserver::MockPrivacySandboxObserver() = default;
MockPrivacySandboxObserver::~MockPrivacySandboxObserver() = default;

void RunTestCase(
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* host_content_settings_map,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider,
    const TestCase& test_case) {
  auto [test_state, test_input, test_output] = test_case;

  // Setup test state.
  for (const auto& [key, value] : UnpackKeys<StateKey>(test_state)) {
    ApplyTestState(key, value, task_environment, testing_pref_service,
                   host_content_settings_map, privacy_sandbox_service,
                   privacy_sandbox_settings, user_content_setting_provider,
                   managed_content_setting_provider);
  }

  // Provide any inputs not directly related to an output function.
  auto inputs = UnpackKeys<InputKey>(test_input);
  for (const auto& input : inputs) {
    ProvideInput(input, privacy_sandbox_service);
  }

  // Check expected outputs for provided inputs matches actual output.
  for (const auto& output : UnpackKeys<OutputKey>(test_output)) {
    CheckOutput(inputs, output, privacy_sandbox_settings,
                privacy_sandbox_service, testing_pref_service);
  }
}

}  // namespace privacy_sandbox_test_util
