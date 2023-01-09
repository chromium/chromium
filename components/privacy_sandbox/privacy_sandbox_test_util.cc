// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_test_util.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
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
    if (absl::holds_alternative<T>(test_key)) {
      auto key = absl::get<T>(test_key);
      EXPECT_EQ(0u, unpacked_map.count(key))
          << "Duplicate test key " << static_cast<int>(key);
      unpacked_map[key] = value;
    } else {
      auto keys = absl::get<MultipleKeys<T>>(test_key);
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
  EXPECT_TRUE(absl::holds_alternative<T>(value));
  return absl::get<T>(value);
}

template <typename V, typename K>
V GetItemValueForKey(K key, std::map<K, TestCaseItemValue> test_components) {
  EXPECT_TRUE(test_components.count(key))
      << "Unable to find key " << static_cast<int>(key);
  return GetItemValue<V>(test_components.at(key));
}

// Applies the state defined by `key`, `value` to the provided profile
// components.
void ApplyTestState(
    StateKey key,
    const TestCaseItemValue& value,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    MockPrivacySandboxSettingsDelegate* mock_delegate,
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
          base::Value(content_setting));
      return;
    }
    case (StateKey::kSiteDataUserExceptions): {
      SCOPED_TRACE("State Setup: User site data exceptions");
      auto exceptions = GetItemValue<SiteDataExceptions>(value);

      for (const auto& [primary_pattern, content_setting] : exceptions) {
        user_content_setting_provider->SetWebsiteSetting(
            ContentSettingsPattern::FromString(primary_pattern),
            ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
            base::Value(content_setting));
      }
      return;
    }
    case (StateKey::kIsIncognito): {
      SCOPED_TRACE("State Setup: User Incognito");
      mock_delegate->SetUpIsIncognitoProfileResponse(GetItemValue<bool>(value));
      return;
    }
    case (StateKey::kIsRestrictedAccount): {
      SCOPED_TRACE("State Setup: User restricted");
      mock_delegate->SetUpIsPrivacySandboxRestrictedResponse(
          GetItemValue<bool>(value));
      return;
    }
    default:
      NOTREACHED();
  }
}

void CheckOutput(
    const std::map<InputKey, TestCaseItemValue>& input,
    const std::pair<OutputKey, TestCaseItemValue>& output,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings) {
  auto [output_key, output_value] = output;
  switch (output_key) {
    case (OutputKey::kIsTopicsAllowed): {
      SCOPED_TRACE("Check Output: IsTopicsAllowed()");
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value, privacy_sandbox_settings->IsTopicsAllowed());
      return;
    }
    case (OutputKey::kIsTopicsAllowedForContext): {
      SCOPED_TRACE("Check Output: IsTopicsAllowedForContext()");
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto topics_url = GetItemValueForKey<GURL>(InputKey::kTopicsURL, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value,
                privacy_sandbox_settings->IsTopicsAllowedForContext(
                    top_frame_origin, topics_url));
      return;
    }
    case (OutputKey::kIsFledgeAllowed): {
      SCOPED_TRACE("Check Output: IsFledgeAllowed()");
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto fledge_auction_party_origin = GetItemValueForKey<url::Origin>(
          InputKey::kFledgeAuctionPartyOrigin, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value,
                privacy_sandbox_settings->IsFledgeAllowed(
                    top_frame_origin, fledge_auction_party_origin));
      return;
    }
    case (OutputKey::kIsAttributionReportingAllowed): {
      SCOPED_TRACE("Check Output: IsAttributionReportingAllowed()");
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto reporting_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementReportingOrigin, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value,
                privacy_sandbox_settings->IsAttributionReportingAllowed(
                    top_frame_origin, reporting_origin));
      return;
    }
    case (OutputKey::kMaySendAttributionReport): {
      SCOPED_TRACE("Check Output: MaySendAttributionReport()");
      auto source_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementSourceOrigin, input);
      auto destination_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementDestinationOrigin, input);
      auto reporting_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementReportingOrigin, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value,
                privacy_sandbox_settings->MaySendAttributionReport(
                    source_origin, destination_origin, reporting_origin));
      return;
    }

    case (OutputKey::kIsSharedStorageAllowed): {
      SCOPED_TRACE("Check Output: kIsSharedStorageAllowed()");
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto accessing_origin =
          GetItemValueForKey<url::Origin>(InputKey::kAccessingOrigin, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value, privacy_sandbox_settings->IsSharedStorageAllowed(
                                  top_frame_origin, accessing_origin));
      return;
    }

    case (OutputKey::kIsSharedStorageSelectURLAllowed): {
      SCOPED_TRACE("Check Output: IsSharedStorageSelectURLAllowed()");
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto accessing_origin =
          GetItemValueForKey<url::Origin>(InputKey::kAccessingOrigin, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value,
                privacy_sandbox_settings->IsSharedStorageSelectURLAllowed(
                    top_frame_origin, accessing_origin));
      return;
    }

    case (OutputKey::kIsPrivateAggregationAllowed): {
      SCOPED_TRACE("Check Output: IsPrivateAggregationAllowed()");
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto reporting_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementReportingOrigin, input);
      auto return_value = GetItemValue<bool>(output_value);
      ASSERT_EQ(return_value,
                privacy_sandbox_settings->IsPrivateAggregationAllowed(
                    top_frame_origin, reporting_origin));
      return;
    }

    case (OutputKey::kIsTopicsAllowedMetric): {
      SCOPED_TRACE("Check Output: PrivacySandbox.IsTopicsAllowed");
      base::HistogramTester histogram_tester;
      std::ignore = privacy_sandbox_settings->IsTopicsAllowed();
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample("PrivacySandbox.IsTopicsAllowed",
                                          histogram_value, 1);
      return;
    }
    case (OutputKey::kIsTopicsAllowedForContextMetric): {
      SCOPED_TRACE("Check Output: PrivacySandbox.IsTopicsAllowedForContext");
      base::HistogramTester histogram_tester;
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto topics_url = GetItemValueForKey<GURL>(InputKey::kTopicsURL, input);
      std::ignore = privacy_sandbox_settings->IsTopicsAllowedForContext(
          top_frame_origin, topics_url);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.IsTopicsAllowedForContext", histogram_value, 1);
      return;
    }
    case (OutputKey::kIsFledgeAllowedMetric): {
      SCOPED_TRACE("Check Output: PrivacySandbox.IsFledgeAllowed");
      base::HistogramTester histogram_tester;
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto fledge_auction_party_origin = GetItemValueForKey<url::Origin>(
          InputKey::kFledgeAuctionPartyOrigin, input);
      std::ignore = privacy_sandbox_settings->IsFledgeAllowed(
          top_frame_origin, fledge_auction_party_origin);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample("PrivacySandbox.IsFledgeAllowed",
                                          histogram_value, 1);
      return;
    }
    case (OutputKey::kIsAttributionReportingAllowedMetric): {
      SCOPED_TRACE(
          "Check Output: PrivacySandbox.IsAttributionReportingAllowed");
      base::HistogramTester histogram_tester;
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto reporting_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementReportingOrigin, input);
      std::ignore = privacy_sandbox_settings->IsAttributionReportingAllowed(
          top_frame_origin, reporting_origin);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.IsAttributionReportingAllowed", histogram_value, 1);
      return;
    }
    case (OutputKey::kMaySendAttributionReportMetric): {
      SCOPED_TRACE("Check Output: PrivacySandbox.MaySendAttributionReport");
      base::HistogramTester histogram_tester;
      auto source_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementSourceOrigin, input);
      auto destination_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementDestinationOrigin, input);
      auto reporting_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementReportingOrigin, input);
      std::ignore = privacy_sandbox_settings->MaySendAttributionReport(
          source_origin, destination_origin, reporting_origin);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.MaySendAttributionReport", histogram_value, 1);
      return;
    }
    case (OutputKey::kIsSharedStorageAllowedMetric): {
      SCOPED_TRACE("Check Output: PrivacySandbox.IsSharedStorageAllowed");
      base::HistogramTester histogram_tester;
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto accessing_origin =
          GetItemValueForKey<url::Origin>(InputKey::kAccessingOrigin, input);
      std::ignore = privacy_sandbox_settings->IsSharedStorageAllowed(
          top_frame_origin, accessing_origin);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.IsSharedStorageAllowed", histogram_value, 1);
      return;
    }
    case (OutputKey::kIsSharedStorageSelectURLAllowedMetric): {
      SCOPED_TRACE(
          "Check Output: PrivacySandbox.IsSharedStorageSelectURLAllowed");
      base::HistogramTester histogram_tester;
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto accessing_origin =
          GetItemValueForKey<url::Origin>(InputKey::kAccessingOrigin, input);
      std::ignore = privacy_sandbox_settings->IsSharedStorageSelectURLAllowed(
          top_frame_origin, accessing_origin);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.IsSharedStorageSelectURLAllowed", histogram_value, 1);
      return;
    }
    case (OutputKey::kIsPrivateAggregationAllowedMetric): {
      SCOPED_TRACE("Check Output: PrivacySandbox.IsPrivateAggregationAllowed");
      base::HistogramTester histogram_tester;
      auto top_frame_origin =
          GetItemValueForKey<url::Origin>(InputKey::kTopFrameOrigin, input);
      auto reporting_origin = GetItemValueForKey<url::Origin>(
          InputKey::kAdMeasurementReportingOrigin, input);
      std::ignore = privacy_sandbox_settings->IsPrivateAggregationAllowed(
          top_frame_origin, reporting_origin);
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.IsPrivateAggregationAllowed", histogram_value, 1);
      return;
    }
  }
}

}  // namespace
MockPrivacySandboxObserver::MockPrivacySandboxObserver() = default;
MockPrivacySandboxObserver::~MockPrivacySandboxObserver() = default;
MockPrivacySandboxSettingsDelegate::MockPrivacySandboxSettingsDelegate() =
    default;
MockPrivacySandboxSettingsDelegate::~MockPrivacySandboxSettingsDelegate() =
    default;

void SetupTestState(
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    bool privacy_sandbox_enabled,
    bool block_third_party_cookies,
    ContentSetting default_cookie_setting,
    const std::vector<CookieContentSettingException>& user_cookie_exceptions,
    ContentSetting managed_cookie_setting,
    const std::vector<CookieContentSettingException>&
        managed_cookie_exceptions) {
  // Setup block-third-party-cookies settings.
  testing_pref_service->SetUserPref(
      prefs::kCookieControlsMode,
      base::Value(static_cast<int>(
          block_third_party_cookies
              ? content_settings::CookieControlsMode::kBlockThirdParty
              : content_settings::CookieControlsMode::kOff)));

  // Setup cookie content settings.
  auto user_provider = std::make_unique<content_settings::MockProvider>();
  auto managed_provider = std::make_unique<content_settings::MockProvider>();

  if (default_cookie_setting != kNoSetting) {
    user_provider->SetWebsiteSetting(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::COOKIES, base::Value(default_cookie_setting));
  }

  for (const auto& exception : user_cookie_exceptions) {
    user_provider->SetWebsiteSetting(
        ContentSettingsPattern::FromString(exception.primary_pattern),
        ContentSettingsPattern::FromString(exception.secondary_pattern),
        ContentSettingsType::COOKIES, base::Value(exception.content_setting));
  }

  if (managed_cookie_setting != kNoSetting) {
    managed_provider->SetWebsiteSetting(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::COOKIES, base::Value(managed_cookie_setting));
  }

  for (const auto& exception : managed_cookie_exceptions) {
    managed_provider->SetWebsiteSetting(
        ContentSettingsPattern::FromString(exception.primary_pattern),
        ContentSettingsPattern::FromString(exception.secondary_pattern),
        ContentSettingsType::COOKIES, base::Value(exception.content_setting));
  }

  content_settings::TestUtils::OverrideProvider(
      map, std::move(user_provider), HostContentSettingsMap::DEFAULT_PROVIDER);
  content_settings::TestUtils::OverrideProvider(
      map, std::move(managed_provider),
      HostContentSettingsMap::POLICY_PROVIDER);

  // Only adjust the Privacy Sandbox preference which should be being consulted
  // based on feature state.
  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3)) {
    testing_pref_service->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                                      base::Value(privacy_sandbox_enabled));
  } else {
    testing_pref_service->SetUserPref(prefs::kPrivacySandboxApisEnabled,
                                      base::Value(privacy_sandbox_enabled));
  }
}

void RunTestCase(
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* host_content_settings_map,
    MockPrivacySandboxSettingsDelegate* mock_delegate,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider,
    const TestCase& test_case) {
  auto [test_state, test_input, test_output] = test_case;

  // Setup test state.
  for (const auto& [key, value] : UnpackKeys<StateKey>(test_state)) {
    ApplyTestState(key, value, testing_pref_service, host_content_settings_map,
                   mock_delegate, user_content_setting_provider,
                   managed_content_setting_provider);
  }

  // Check expected outputs for provided inputs matches actual output.
  auto inputs = UnpackKeys<InputKey>(test_input);
  for (const auto& output : UnpackKeys<OutputKey>(test_output)) {
    CheckOutput(inputs, output, privacy_sandbox_settings);
  }
}

}  // namespace privacy_sandbox_test_util
