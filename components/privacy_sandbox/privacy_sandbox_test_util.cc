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
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"
namespace privacy_sandbox_test_util {

namespace {

constexpr int kTestTaxonomyVersion = 1;

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

}  // namespace

void ApplyTestState(
    StateKey key,
    const TestCaseItemValue& value,
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    MockPrivacySandboxSettingsDelegate* mock_delegate,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service,
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
    case (StateKey::kHasCurrentTopics): {
      auto has_current_topics = GetItemValue<bool>(value);
      if (!has_current_topics) {
        // By default, there are no blocked topics.
        return;
      }
      const auto kTopic = privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(24),  // "Blues"
          kTestTaxonomyVersion);
      const std::vector<privacy_sandbox::CanonicalTopic> topics = {kTopic};

      EXPECT_CALL(*mock_browsing_topics_service, GetTopTopicsForDisplay())
          .WillRepeatedly(testing::Return(topics));
      return;
    }
    case (StateKey::kHasBlockedTopics): {
      auto has_current_topics = GetItemValue<bool>(value);
      if (!has_current_topics) {
        // By default, there are no current topics.
        return;
      }
      const auto kTopic = privacy_sandbox::CanonicalTopic(
          browsing_topics::Topic(25),  // "Classical Music"
          kTestTaxonomyVersion);
      privacy_sandbox_service->SetTopicAllowed(kTopic, false);
      return;
    }
    case (StateKey::kAdvanceClockBy): {
      auto time_delta = GetItemValue<base::TimeDelta>(value);
      task_environment->AdvanceClock(time_delta);
      return;
    }
    case (StateKey::kActiveTopicsConsent): {
      bool active_consent = GetItemValue<bool>(value);
      testing_pref_service->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven,
                                       active_consent);

      // For other values associated with consent, use values which are
      // arbitrary, but won't be expected by any test, and don't affect whether
      // the consent is considered active.
      testing_pref_service->SetTime(
          prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
          base::Time::Now() - base::Microseconds(12345));
      testing_pref_service->SetInteger(
          prefs::kPrivacySandboxTopicsConsentLastUpdateReason, 1234);
      testing_pref_service->SetString(
          prefs::kPrivacySandboxTopicsConsentTextAtLastUpdate, "Foo Bar Baz");
      return;
    }
    case (StateKey::kApisEnabledV2): {
      SCOPED_TRACE("State Setup: Privacy Sandbox Apis enabled");
      testing_pref_service->SetUserPref(prefs::kPrivacySandboxApisEnabledV2,
                                        base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kTrialsConsentDecisionMade): {
      SCOPED_TRACE("State Setup: Trials consent decision made");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxConsentDecisionMade,
          base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kTrialsNoticeDisplayed): {
      SCOPED_TRACE("State Setup: Trials notice displayed");
      testing_pref_service->SetUserPref(prefs::kPrivacySandboxNoticeDisplayed,
                                        base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1ConsentDecisionMade): {
      SCOPED_TRACE("State Setup: M1 consent decision made");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxM1ConsentDecisionMade,
          base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1EEANoticeAcknowledged): {
      SCOPED_TRACE("State Setup: M1 eea notice acknowledged");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxM1EEANoticeAcknowledged,
          base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1RowNoticeAcknowledged): {
      SCOPED_TRACE("State Setup: M1 row notice acknowledged");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxM1RowNoticeAcknowledged,
          base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1RestrictedNoticeAcknowledged): {
      SCOPED_TRACE("State Setup: M1 restricted notice acknowledged");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged,
          base::Value(GetItemValue<bool>(value)));
      return;
    }
    case (StateKey::kM1PromptSuppressedReason): {
      SCOPED_TRACE("State Setup: M1 prompt suppressed value");
      testing_pref_service->SetUserPref(
          prefs::kPrivacySandboxM1PromptSuppressed,
          base::Value(GetItemValue<int>(value)));
      return;
    }
    case (StateKey::kM1PromptDisabledByPolicy): {
      SCOPED_TRACE("State Setup: M1 prompt disabled by policy");
      testing_pref_service->SetManagedPref(
          prefs::kPrivacySandboxM1PromptSuppressed,
          base::Value(GetItemValue<int>(value)));
      EXPECT_TRUE(testing_pref_service->IsManagedPreference(
          prefs::kPrivacySandboxM1PromptSuppressed));
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
    case (StateKey::kHasAppropriateTopicsConsent): {
      SCOPED_TRACE("State Setup: Appropriate Topics Consent");
      mock_delegate->SetUpHasAppropriateTopicsConsentResponse(
          GetItemValue<bool>(value));
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
    case (InputKey::kTopicsToggleNewValue): {
      privacy_sandbox_service->TopicsToggleChanged(
          GetItemValue<bool>(input_value));
      return;
    }
    case (InputKey::kPromptAction): {
      privacy_sandbox_service->PromptActionOccurred(
          GetItemValue<int>(input_value));
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
    case (OutputKey::kTopicsConsentGiven): {
      SCOPED_TRACE("Check Output: Topics Consent Given");
      auto consent_given = GetItemValue<bool>(output_value);
      EXPECT_EQ(consent_given,
                privacy_sandbox_service->TopicsHasActiveConsent());
      return;
    }
    case (OutputKey::kTopicsConsentLastUpdateReason): {
      SCOPED_TRACE("Check Output: Topics Consent Update Source");
      auto consent_update_source =
          GetItemValue<privacy_sandbox::TopicsConsentUpdateSource>(
              output_value);
      EXPECT_EQ(consent_update_source,
                privacy_sandbox_service->TopicsConsentLastUpdateSource());
      return;
    }
    case (OutputKey::kTopicsConsentLastUpdateTime): {
      SCOPED_TRACE("Check Output: Topics Consent Last Update Time");
      auto consent_last_update = GetItemValue<base::Time>(output_value);
      EXPECT_EQ(consent_last_update,
                privacy_sandbox_service->TopicsConsentLastUpdateTime());
      return;
    }
    case (OutputKey::kTopicsConsentStringIdentifiers): {
      SCOPED_TRACE("Check Output: Topics Consent String Identifiers");

      auto string_ids = GetItemValue<std::vector<int>>(output_value);

      std::string stored_text =
          privacy_sandbox_service->TopicsConsentLastUpdateText();

      // The stored text should contain all of the strings specified by
      // `string_ids` in order, each separated by a single space. We can
      // verify this by finding each string in `stored_text`, starting from
      // the end of where the previous string was found.
      auto stored_text_iterator = stored_text.begin();

      for (auto string_id : string_ids) {
        auto string = l10n_util::GetStringUTF8(string_id);
        base::ReplaceSubstringsAfterOffset(&string, 0, "<b>", "");
        base::ReplaceSubstringsAfterOffset(&string, 0, "</b>", "");
        SCOPED_TRACE(
            "Expecting to find: \"" + string + "\" at the start of \"" +
            std::string(stored_text_iterator, stored_text.end()) + "\"");

        auto mismatch_pair =
            base::ranges::mismatch(string.begin(), string.end(),
                                   stored_text_iterator, stored_text.end());

        // The first mismatch should be at the end of the string, indicating
        // that the entire string was matched.
        EXPECT_EQ(string.end(), mismatch_pair.first);

        // Update text iterator to where the matches for this string stopped.
        stored_text_iterator = mismatch_pair.second;

        // The iterator should now point to the whitespace character joining the
        // strings, unless we're at the end of the string.
        if (stored_text_iterator != stored_text.end()) {
          EXPECT_EQ(' ', *stored_text_iterator);
          stored_text_iterator++;
        }
      }
      return;
    }
    case (OutputKey::kPromptType): {
      SCOPED_TRACE("Check Output: PrivacySandboxService.GetRequiredPromptType");
      auto prompt_type = GetItemValue<int>(output_value);
      auto force_chrome_build =
          GetItemValueForKey<bool>(InputKey::kForceChromeBuild, input);
      privacy_sandbox_service->ForceChromeBuildForTests(force_chrome_build);
      EXPECT_EQ(prompt_type, privacy_sandbox_service->GetRequiredPromptType());
      return;
    }
    case (OutputKey::kM1PromptSuppressedReason): {
      SCOPED_TRACE("Check Output: Prompt suppressed reason");
      auto prompt_suppressed_reason = GetItemValue<int>(output_value);
      auto force_chrome_build =
          GetItemValueForKey<bool>(InputKey::kForceChromeBuild, input);
      privacy_sandbox_service->ForceChromeBuildForTests(force_chrome_build);
      EXPECT_EQ(prompt_suppressed_reason,
                testing_pref_service->GetInteger(
                    prefs::kPrivacySandboxM1PromptSuppressed));
      return;
    }
    case (OutputKey::kM1ConsentDecisionMade): {
      SCOPED_TRACE("Check Output: M1 consent decision made");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected, testing_pref_service->GetBoolean(
                              prefs::kPrivacySandboxM1ConsentDecisionMade));
      return;
    }
    case (OutputKey::kM1EEANoticeAcknowledged): {
      SCOPED_TRACE("Check Output: M1 eea notice acknowledged");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected, testing_pref_service->GetBoolean(
                              prefs::kPrivacySandboxM1EEANoticeAcknowledged));
      return;
    }
    case (OutputKey::kM1RowNoticeAcknowledged): {
      SCOPED_TRACE("Check Output: M1 row notice acknowledged");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected, testing_pref_service->GetBoolean(
                              prefs::kPrivacySandboxM1RowNoticeAcknowledged));
      return;
    }
    case (OutputKey::kM1RestrictedNoticeAcknowledged): {
      SCOPED_TRACE("Check Output: M1 restricted notice acknowledged");
      bool expected = GetItemValue<bool>(output_value);
      EXPECT_EQ(expected,
                testing_pref_service->GetBoolean(
                    prefs::kPrivacySandboxM1RestrictedNoticeAcknowledged));
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
    case (OutputKey::kIsAttributionReportingEverAllowed): {
      SCOPED_TRACE("Check Output: Is Attribution Reporting Ever Allowed");
      bool expected = GetItemValue<bool>(output_value);
      ASSERT_EQ(expected,
                privacy_sandbox_settings->IsAttributionReportingEverAllowed());
      return;
    }
    case (OutputKey::kIsAttributionReportingEverAllowedMetric): {
      SCOPED_TRACE(
          "Check Output: PrivacySandbox.IsAttributionReportingEverAllowed");
      base::HistogramTester histogram_tester;
      std::ignore =
          privacy_sandbox_settings->IsAttributionReportingEverAllowed();
      auto histogram_value = GetItemValue<int>(output_value);
      histogram_tester.ExpectUniqueSample(
          "PrivacySandbox.IsAttributionReportingEverAllowed", histogram_value,
          1);
      return;
    }
  }
}

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
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* host_content_settings_map,
    MockPrivacySandboxSettingsDelegate* mock_delegate,
    browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service_,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider,
    const TestCase& test_case) {
  auto [test_state, test_input, test_output] = test_case;

  // Setup test state.
  for (const auto& [key, value] : UnpackKeys<StateKey>(test_state)) {
    ApplyTestState(key, value, task_environment, testing_pref_service,
                   host_content_settings_map, mock_delegate,
                   privacy_sandbox_service, mock_browsing_topics_service_,
                   user_content_setting_provider,
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
