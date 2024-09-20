// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_

#include <set>
#include <string>

#include "components/browsing_topics/test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

class HostContentSettingsMap;

namespace privacy_sandbox_test_util {

class PrivacySandboxServiceTestInterface {
 public:
  virtual void TopicsToggleChanged(bool new_value) const = 0;
  virtual void SetTopicAllowed(privacy_sandbox::CanonicalTopic topic,
                               bool allowed) = 0;
  virtual bool TopicsHasActiveConsent() const = 0;
  virtual privacy_sandbox::TopicsConsentUpdateSource
  TopicsConsentLastUpdateSource() const = 0;
  virtual base::Time TopicsConsentLastUpdateTime() const = 0;
  virtual std::string TopicsConsentLastUpdateText() const = 0;
  virtual void ForceChromeBuildForTests(bool force_chrome_build) const = 0;
  virtual int GetRequiredPromptType(int surface_type) const = 0;
  virtual void PromptActionOccurred(int action, int surface_type) const = 0;
};

class MockPrivacySandboxObserver
    : public privacy_sandbox::PrivacySandboxSettings::Observer {
 public:
  MockPrivacySandboxObserver();
  ~MockPrivacySandboxObserver();
  MOCK_METHOD(void, OnTopicsDataAccessibleSinceUpdated, (), (override));
  MOCK_METHOD1(OnFirstPartySetsEnabledChanged, void(bool));
};

class MockPrivacySandboxSettingsDelegate
    : public privacy_sandbox::PrivacySandboxSettings::Delegate {
 public:
  MockPrivacySandboxSettingsDelegate();
  ~MockPrivacySandboxSettingsDelegate() override;
  void SetUpIsPrivacySandboxRestrictedResponse(bool restricted) {
    ON_CALL(*this, IsPrivacySandboxRestricted).WillByDefault([=]() {
      return restricted;
    });
  }

  void SetUpIsPrivacySandboxCurrentlyUnrestrictedResponse(bool unrestricted) {
    ON_CALL(*this, IsPrivacySandboxCurrentlyUnrestricted).WillByDefault([=]() {
      return unrestricted;
    });
  }

  void SetUpIsIncognitoProfileResponse(bool incognito) {
    ON_CALL(*this, IsIncognitoProfile).WillByDefault([=]() {
      return incognito;
    });
  }

  void SetUpHasAppropriateTopicsConsentResponse(bool has_appropriate_consent) {
    ON_CALL(*this, HasAppropriateTopicsConsent).WillByDefault([=]() {
      return has_appropriate_consent;
    });
  }

  void SetUpIsSubjectToM1NoticeRestrictedResponse(
      bool is_subject_to_restricted_notice) {
    ON_CALL(*this, IsSubjectToM1NoticeRestricted).WillByDefault([=]() {
      return is_subject_to_restricted_notice;
    });
  }

  void SetUpIsCookieDeprecationExperimentEligibleResponse(bool eligible) {
    ON_CALL(*this, IsCookieDeprecationExperimentEligible).WillByDefault([=]() {
      return eligible;
    });
  }

  void SetUpGetCookieDeprecationExperimentCurrentEligibility(
      privacy_sandbox::TpcdExperimentEligibility::Reason eligibility_reason) {
    ON_CALL(*this, GetCookieDeprecationExperimentCurrentEligibility)
        .WillByDefault([=]() {
          return privacy_sandbox::TpcdExperimentEligibility(eligibility_reason);
        });
  }

  void SetUpIsCookieDeprecationLabelAllowedResponse(bool allowed) {
    ON_CALL(*this, IsCookieDeprecationLabelAllowed).WillByDefault([=]() {
      return allowed;
    });
  }

  void SetUpAreThirdPartyCookiesBlockedByCookieDeprecationExperimentResponse(
      bool result) {
    ON_CALL(*this, AreThirdPartyCookiesBlockedByCookieDeprecationExperiment)
        .WillByDefault([=]() { return result; });
  }

  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (const, override));
  MOCK_METHOD(bool,
              IsPrivacySandboxCurrentlyUnrestricted,
              (),
              (const, override));

  MOCK_METHOD(bool, IsIncognitoProfile, (), (const, override));
  MOCK_METHOD(bool, HasAppropriateTopicsConsent, (), (const, override));
  MOCK_METHOD(bool, IsSubjectToM1NoticeRestricted, (), (const, override));
  MOCK_METHOD(bool, IsRestrictedNoticeEnabled, (), (const, override));
  MOCK_METHOD(bool,
              IsCookieDeprecationExperimentEligible,
              (),
              (const, override));
  MOCK_METHOD(privacy_sandbox::TpcdExperimentEligibility,
              GetCookieDeprecationExperimentCurrentEligibility,
              (),
              (const, override));
  MOCK_METHOD(bool, IsCookieDeprecationLabelAllowed, (), (const, override));
  MOCK_METHOD(bool,
              AreThirdPartyCookiesBlockedByCookieDeprecationExperiment,
              (),
              (const, override));
};

// A declarative test case is a collection of key value pairs, which each define
// some property of the test, such as the state of the profile, the input, or
// expected output.
// Defines the state of the profile prior to testing.
enum class StateKey {
  kM1TopicsEnabledUserPrefValue = 1,
  kCookieControlsModeUserPrefValue = 2,
  kSiteDataUserDefault = 3,
  kSiteDataUserExceptions = 4,
  kM1FledgeEnabledUserPrefValue = 5,
  kM1AdMeasurementEnabledUserPrefValue = 6,
  kIsIncognito = 7,
  kIsRestrictedAccount = 8,
  kHasCurrentTopics = 9,
  kHasBlockedTopics = 10,
  kAdvanceClockBy = 11,
  kActiveTopicsConsent = 12,
  kTrialsConsentDecisionMade = 14,
  kTrialsNoticeDisplayed = 15,
  kM1ConsentDecisionPreviouslyMade = 16,
  kM1EEANoticePreviouslyAcknowledged = 17,
  kM1RowNoticePreviouslyAcknowledged = 18,
  kM1PromptPreviouslySuppressedReason = 19,
  kM1PromptDisabledByPolicy = 20,
  kM1TopicsDisabledByPolicy = 21,
  kM1FledgeDisabledByPolicy = 22,
  kM1AdMesaurementDisabledByPolicy = 23,
  kHasAppropriateTopicsConsent = 24,
  kM1RestrictedNoticePreviouslyAcknowledged = 25,
  kAttestationsMap = 26,
  kBlockFledgeJoiningForEtldplus1 = 27,
  kBlockAll3pcToggleEnabledUserPrefValue = 28,
  kTrackingProtection3pcdEnabledUserPrefValue = 29,
};

// Defines the input to the functions under test.
enum class InputKey {
  kTopFrameOrigin = 1,
  kTopicsURL = 2,
  kFledgeAuctionPartyOrigin = 3,
  kAdMeasurementReportingOrigin = 4,
  kAdMeasurementSourceOrigin = 5,
  kAdMeasurementDestinationOrigin = 6,
  kAccessingOrigin = 7,
  kTopicsToggleNewValue = 8,
  kForceChromeBuild = 9,
  kPromptAction = 10,
  kEventReportingDestinationOrigin = 11,
  kOutSharedStorageDebugMessage = 12,
  kOutSharedStorageSelectURLDebugMessage = 13,
  kOutSharedStorageBlockIsSiteSettingSpecific = 14,
  kOutSharedStorageSelectURLBlockIsSiteSettingSpecific = 15,
  kOutPrivateAggregationBlockIsSiteSettingSpecific = 16,
};

// Defines the expected output of the functions under test, when the profile is
// setup as per defined state, and they are provided the defined inputs.
enum class OutputKey {
  kIsTopicsAllowed = 1,
  kIsTopicsAllowedForContext = 2,
  kIsAttributionReportingAllowed = 4,
  kMaySendAttributionReport = 5,
  kIsSharedStorageAllowed = 6,
  kIsSharedStorageSelectURLAllowed = 7,
  kIsPrivateAggregationAllowed = 8,
  kIsTopicsAllowedMetric = 9,
  kIsTopicsAllowedForContextMetric = 10,
  kIsAttributionReportingAllowedMetric = 12,
  kMaySendAttributionReportMetric = 13,
  kIsSharedStorageAllowedMetric = 14,
  kIsSharedStorageSelectURLAllowedMetric = 15,
  kIsPrivateAggregationAllowedMetric = 16,
  kTopicsConsentGiven = 17,
  kTopicsConsentLastUpdateReason = 18,
  kTopicsConsentLastUpdateTime = 19,
  kTopicsConsentStringIdentifiers = 20,
  kPromptType = 21,
  kM1PromptSuppressedReason = 22,
  kM1ConsentDecisionMade = 23,
  kM1EEANoticeAcknowledged = 24,
  kM1RowNoticeAcknowledged = 25,
  kM1TopicsEnabled = 26,
  kM1FledgeEnabled = 27,
  kM1AdMeasurementEnabled = 28,
  kIsAttributionReportingEverAllowed = 29,
  kIsAttributionReportingEverAllowedMetric = 30,
  kM1RestrictedNoticeAcknowledged = 31,
  kIsEventReportingDestinationAttestedForFledge = 32,
  kIsEventReportingDestinationAttestedForSharedStorage = 33,
  kIsEventReportingDestinationAttestedForFledgeMetric = 34,
  kIsEventReportingDestinationAttestedForSharedStorageMetric = 35,
  kIsFledgeJoinAllowed = 36,
  kIsFledgeLeaveAllowed = 37,
  kIsFledgeUpdateAllowed = 38,
  kIsFledgeSellAllowed = 39,
  kIsFledgeBuyAllowed = 40,
  kIsFledgeJoinAllowedMetric = 41,
  kIsFledgeLeaveAllowedMetric = 42,
  kIsFledgeUpdateAllowedMetric = 43,
  kIsFledgeSellAllowedMetric = 44,
  kIsFledgeBuyAllowedMetric = 45,
  kIsCookieDeprecationLabelAllowedForContext = 46,
  kIsPrivateAggregationDebugModeAllowed = 47,
  kIsSharedStorageAllowedDebugMessage = 48,
  kIsSharedStorageSelectURLAllowedDebugMessage = 49,
  kIsSharedStorageBlockSiteSettingSpecific = 50,
  kIsSharedStorageSelectURLBlockSiteSettingSpecific = 51,
  kIsPrivateAggregationBlockSiteSettingSpecific = 52,
  kIsLocalUnpartitionedDataAccessAllowed = 53,
  kIsLocalUnpartitionedDataAccessAllowedMetric = 54,
};

// To allow multiple input keys to map to the same value, without having to
// redeclare every such relationship, additional types are defined here. The
// result is that `TestKey` can represent 1:1 and many:1 key value
// relationships.
template <typename T>
using MultipleKeys = std::set<T>;

using MultipleStateKeys = MultipleKeys<StateKey>;
using MultipleInputKeys = MultipleKeys<InputKey>;
using MultipleOutputKeys = MultipleKeys<OutputKey>;

template <typename T>
using TestKey = absl::variant<T, MultipleKeys<T>>;

using SiteDataException = std::pair<std::string, ContentSetting>;
using SiteDataExceptions = std::vector<SiteDataException>;

// Although each part of the test case (state, input, output) uses different
// key types, the set of value types associated with those keys is shared, and
// represented by this variant. When accessing keys, the test util will expect
// a particular value type, and will error otherwise.
using TestCaseItemValue = absl::variant<
    bool,
    bool*,
    std::string,
    std::string*,
    url::Origin,
    GURL,
    content_settings::CookieControlsMode,
    SiteDataExceptions,
    ContentSetting,
    int,
    base::Time,
    base::TimeDelta,
    privacy_sandbox::TopicsConsentUpdateSource,
    std::vector<int>,
    std::optional<privacy_sandbox::PrivacySandboxAttestationsMap>>;

using TestState = std::map<TestKey<StateKey>, TestCaseItemValue>;
using TestInput = std::map<TestKey<InputKey>, TestCaseItemValue>;
using TestOutput = std::map<TestKey<OutputKey>, TestCaseItemValue>;

using TestCase = std::tuple<TestState, TestInput, TestOutput>;

// Define an additional content setting value to simulate an unmanaged default
// content setting.
const ContentSetting kNoSetting = static_cast<ContentSetting>(-1);

struct CookieContentSettingException {
  std::string primary_pattern;
  std::string secondary_pattern;
  ContentSetting content_setting;
};

// Setup and run the provided test case.
void RunTestCase(
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* host_content_settings_map,
    MockPrivacySandboxSettingsDelegate* mock_delegate,
    browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider,
    const TestCase& test_case);

// Applies the state defined by `key`, `value` to the provided profile
// components. This is only exposed for access via the TestUtil unittest.
// Use `RunTestCase()` exclusively elsewhere.
void ApplyTestState(
    StateKey key,
    const TestCaseItemValue& value,
    content::BrowserTaskEnvironment* task_environment,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service,
    HostContentSettingsMap* map,
    MockPrivacySandboxSettingsDelegate* mock_delegate,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    content_settings::MockProvider* user_content_setting_provider,
    content_settings::MockProvider* managed_content_setting_provider);

// Some input is not directly passed to the function under test, and so must
// be run in advance of checking output. When input is provided directly to
// and output function, it is handled in `CheckOutput()`. This is only exposed
// for access via the TestUtil unit test. Use `RunTestCase()` exclusively
// elsewhere.
void ProvideInput(const std::pair<InputKey, TestCaseItemValue>& input,
                  PrivacySandboxServiceTestInterface* privacy_sandbox_service);

// Checks that the output of functions defined in `output`, when provided with
// appropriate entries from `input` is as expected. This is only exposed for
// access via the TestUtil unit test. Use `RunTestCase()` exclusively elsewhere.
void CheckOutput(
    const std::map<InputKey, TestCaseItemValue>& input,
    const std::pair<OutputKey, TestCaseItemValue>& output,
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    PrivacySandboxServiceTestInterface* privacy_sandbox_service,
    sync_preferences::TestingPrefServiceSyncable* testing_pref_service);

}  // namespace privacy_sandbox_test_util

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
