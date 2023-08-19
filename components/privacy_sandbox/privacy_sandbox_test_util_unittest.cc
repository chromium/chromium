// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_test_util.h"

#include "base/test/task_environment.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/mock_privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace privacy_sandbox_test_util {

namespace {

class MockPrivacySandboxServiceTestInterface
    : public PrivacySandboxServiceTestInterface {
 public:
  MOCK_METHOD(void, TopicsToggleChanged, (bool), (override, const));
  MOCK_METHOD(void,
              SetTopicAllowed,
              (privacy_sandbox::CanonicalTopic, bool),
              (override));
  MOCK_METHOD(bool, TopicsHasActiveConsent, (), (override, const));
  MOCK_METHOD(privacy_sandbox::TopicsConsentUpdateSource,
              TopicsConsentLastUpdateSource,
              (),
              (override, const));
  MOCK_METHOD(base::Time, TopicsConsentLastUpdateTime, (), (override, const));
  MOCK_METHOD(std::string, TopicsConsentLastUpdateText, (), (override, const));
  MOCK_METHOD(void, ForceChromeBuildForTests, (bool), (override, const));
  MOCK_METHOD(int, GetRequiredPromptType, (), (override, const));
  MOCK_METHOD(void, PromptActionOccurred, (int), (override, const));
};

}  // namespace

// TODO (crbug.com/1408187): Add coverage for all state / input / output keys.
class PrivacySandboxTestUtilTest : public testing::Test {
 public:
  PrivacySandboxTestUtilTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting()) {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    host_content_settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cookie_settings_ = new content_settings::CookieSettings(
        host_content_settings_map_.get(), &prefs_, false, "chrome-extension");
  }

  ~PrivacySandboxTestUtilTest() override {
    host_content_settings_map()->ShutdownOnUIThread();
  }

  void SetUp() override {
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    user_provider_ = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    managed_provider_ = managed_provider.get();

    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        HostContentSettingsMap::DEFAULT_PROVIDER);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        HostContentSettingsMap::POLICY_PROVIDER);
  }

 protected:
  void ApplyTestState(StateKey key, const TestCaseItemValue& value) {
    privacy_sandbox_test_util::ApplyTestState(
        key, value, task_environment(), prefs(), host_content_settings_map(),
        mock_delegate(), mock_privacy_sandbox_service(),
        mock_browsing_topics_service(), mock_privacy_sandbox_settings(),
        user_provider_, managed_provider_);
  }

  void ProvideInput(InputKey key, TestCaseItemValue value) {
    privacy_sandbox_test_util::ProvideInput(std::make_pair(key, value),
                                            mock_privacy_sandbox_service());
  }

  void CheckOutput(const std::map<InputKey, TestCaseItemValue>& input,
                   const std::pair<OutputKey, TestCaseItemValue>& output) {
    privacy_sandbox_test_util::CheckOutput(
        input, output, mock_privacy_sandbox_settings(),
        mock_privacy_sandbox_service(), prefs());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }
  content::BrowserTaskEnvironment* task_environment() {
    return &browser_task_environment_;
  }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return &mock_browsing_topics_service_;
  }
  privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate*
  mock_delegate() {
    return &mock_delegate_;
  }
  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }
  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }
  MockPrivacySandboxServiceTestInterface* mock_privacy_sandbox_service() {
    return &mock_privacy_sandbox_service_;
  }
  MockPrivacySandboxSettings* mock_privacy_sandbox_settings() {
    return &mock_privacy_sandbox_settings_;
  }
  content_settings::MockProvider* user_provider() { return user_provider_; }
  content_settings::MockProvider* managed_provider() {
    return managed_provider_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  MockPrivacySandboxSettingsDelegate mock_delegate_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;
  MockPrivacySandboxServiceTestInterface mock_privacy_sandbox_service_;
  MockPrivacySandboxSettings mock_privacy_sandbox_settings_;
  raw_ptr<content_settings::MockProvider> user_provider_;
  raw_ptr<content_settings::MockProvider> managed_provider_;
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations_;
};

TEST_F(PrivacySandboxTestUtilTest, StateKey_M1TopicsEnabledUserPrefValue) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    ApplyTestState(StateKey::kM1TopicsEnabledUserPrefValue, state);
    EXPECT_EQ(
        state,
        prefs()->GetUserPref(prefs::kPrivacySandboxM1TopicsEnabled)->GetBool());
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_CookieControlsModeUserPrefValue) {
  std::vector<content_settings::CookieControlsMode> states = {
      content_settings::CookieControlsMode::kBlockThirdParty,
      content_settings::CookieControlsMode::kIncognitoOnly,
      content_settings::CookieControlsMode::kOff};

  for (auto state : states) {
    ApplyTestState(StateKey::kCookieControlsModeUserPrefValue, state);
    EXPECT_EQ(state,
              static_cast<content_settings::CookieControlsMode>(
                  prefs()->GetUserPref(prefs::kCookieControlsMode)->GetInt()));
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_SiteDataUserDefault) {
  std::vector<ContentSetting> states = {CONTENT_SETTING_ALLOW,
                                        CONTENT_SETTING_BLOCK,
                                        CONTENT_SETTING_SESSION_ONLY};

  for (auto state : states) {
    ApplyTestState(StateKey::kSiteDataUserDefault, state);

    // The state should have ended up in the user provider we gave to the util.
    auto user_rule_iterator =
        user_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                         /*incognito=*/false);

    EXPECT_TRUE(user_rule_iterator->HasNext());
    auto rule = user_rule_iterator->Next();
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
    EXPECT_EQ(base::Value(state), rule->value());

    // Nothing should have ended up in the managed provider, which will present
    // as a null iterator.
    auto managed_rule_iterator =
        managed_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                            /*incognito=*/false);
    EXPECT_EQ(nullptr, managed_rule_iterator);
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_SiteDataUserExceptions) {
  const std::string kException = "https://embedded.com";
  ApplyTestState(StateKey::kSiteDataUserExceptions,
                 SiteDataExceptions{{kException, CONTENT_SETTING_BLOCK}});

  // The state should have ended up in the user provider we gave to the util.
  auto user_rule_iterator =
      user_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                       /*incognito=*/false);

  EXPECT_TRUE(user_rule_iterator->HasNext());
  auto rule = user_rule_iterator->Next();
  EXPECT_EQ(kException, rule->primary_pattern.ToString());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(base::Value(CONTENT_SETTING_BLOCK), rule->value());

  // Nothing should have ended up in the managed provider, which will present
  // as a null iterator.
  auto managed_rule_iterator =
      managed_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                          /*incognito=*/false);
  EXPECT_EQ(nullptr, managed_rule_iterator);
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_M1FledgeEnabledUserPrefValue) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    ApplyTestState(StateKey::kM1FledgeEnabledUserPrefValue, state);
    EXPECT_EQ(
        state,
        prefs()->GetUserPref(prefs::kPrivacySandboxM1FledgeEnabled)->GetBool());
  }
}

TEST_F(PrivacySandboxTestUtilTest,
       StateKey_M1AdMeasurementEnabledUserPrefValue) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    ApplyTestState(StateKey::kM1AdMeasurementEnabledUserPrefValue, state);
    EXPECT_EQ(state,
              prefs()
                  ->GetUserPref(prefs::kPrivacySandboxM1AdMeasurementEnabled)
                  ->GetBool());
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_IsIncognito) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    ApplyTestState(StateKey::kIsIncognito, state);
    EXPECT_EQ(state, mock_delegate()->IsIncognitoProfile());
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_IsRestrictedAccount) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    ApplyTestState(StateKey::kIsRestrictedAccount, state);
    EXPECT_EQ(state, mock_delegate()->IsPrivacySandboxRestricted());
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_HasCurrentTopics) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    ApplyTestState(StateKey::kHasCurrentTopics, state);
    EXPECT_GT(mock_browsing_topics_service()->GetTopTopicsForDisplay().size(),
              0u);
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_HasBlockedTopics) {
  std::vector<bool> states = {true, false};
  for (auto state : states) {
    testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
    EXPECT_CALL(*mock_privacy_sandbox_service(),
                SetTopicAllowed(testing::_, false))
        .Times(state ? 1 : 0);
    ApplyTestState(StateKey::kHasBlockedTopics, state);
  }
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_AdvanceClockBy) {
  base::Time start_time = base::Time::Now();
  ApplyTestState(StateKey::kAdvanceClockBy, base::Hours(1));
  EXPECT_EQ(start_time + base::Hours(1), base::Time::Now());
}

TEST_F(PrivacySandboxTestUtilTest, StateKey_ActiveTopicsConsent) {
  std::vector<bool> states = {true, false};
  for (bool state : states) {
    ApplyTestState(StateKey::kActiveTopicsConsent, state);
    EXPECT_EQ(state, prefs()
                         ->GetUserPref(prefs::kPrivacySandboxTopicsConsentGiven)
                         ->GetBool());
  }
}

TEST_F(PrivacySandboxTestUtilTest, InputKey_TopicsToggleNewValue) {
  std::vector<bool> states = {true, false};
  for (bool state : states) {
    testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
    EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsToggleChanged(state));
    ProvideInput(InputKey::kTopicsToggleNewValue, state);
  }
}

TEST_F(PrivacySandboxTestUtilTest, InputKey_PromptActionOccurred) {
  constexpr int kArbitraryValue = 7;
  testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(kArbitraryValue));
  ProvideInput(InputKey::kPromptAction, kArbitraryValue);
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsTopicsAllowedForContext) {
  GURL kTopicsURL = GURL("https://topics.com");
  url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top-frame.com"));
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsTopicsAllowedForContext(kTopFrameOrigin, kTopicsURL, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kTopicsURL, kTopicsURL},
               {InputKey::kTopFrameOrigin, kTopFrameOrigin}},
              {OutputKey::kIsTopicsAllowedForContext, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsTopicsAllowed) {
  EXPECT_CALL(*mock_privacy_sandbox_settings(), IsTopicsAllowed())
      .WillOnce(testing::Return(true));
  CheckOutput({}, {OutputKey::kIsTopicsAllowed, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsFledgeAllowed) {
  url::Origin kFledgeAuctionPartyOrigin =
      url::Origin::Create(GURL("https://fledge.com"));
  url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top-frame.com"));
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsFledgeAllowed(kTopFrameOrigin, kFledgeAuctionPartyOrigin,
                      content::InterestGroupApiOperation::kJoin, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kFledgeAuctionPartyOrigin, kFledgeAuctionPartyOrigin},
               {InputKey::kTopFrameOrigin, kTopFrameOrigin}},
              {OutputKey::kIsFledgeJoinAllowed, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsAttributionReportingAllowed) {
  url::Origin kAdMeasurementReportingOrigin =
      url::Origin::Create(GURL("https://measurement.com"));
  url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top-frame.com"));
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsAttributionReportingAllowed(
                  kTopFrameOrigin, kAdMeasurementReportingOrigin, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput(
      {{InputKey::kAdMeasurementReportingOrigin, kAdMeasurementReportingOrigin},
       {InputKey::kTopFrameOrigin, kTopFrameOrigin}},
      {OutputKey::kIsAttributionReportingAllowed, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_MaySendAttributionReport) {
  url::Origin kAdMeasurementSourceOrigin =
      url::Origin::Create(GURL("https://source.com"));
  url::Origin kAdMeasurementDestinationOrigin =
      url::Origin::Create(GURL("https://dest.com"));
  url::Origin kAdMeasurementReportingOrigin =
      url::Origin::Create(GURL("https://reporting.com"));
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              MaySendAttributionReport(kAdMeasurementSourceOrigin,
                                       kAdMeasurementDestinationOrigin,
                                       kAdMeasurementReportingOrigin, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput(
      {{InputKey::kAdMeasurementSourceOrigin, kAdMeasurementSourceOrigin},
       {InputKey::kAdMeasurementDestinationOrigin,
        kAdMeasurementDestinationOrigin},
       {InputKey::kAdMeasurementReportingOrigin,
        kAdMeasurementReportingOrigin}},
      {OutputKey::kMaySendAttributionReport, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsSharedStorageAllowed) {
  url::Origin kAccessingOrigin =
      url::Origin::Create(GURL("https://storage.com"));
  url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top-frame.com"));
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsSharedStorageAllowed(kTopFrameOrigin, kAccessingOrigin, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kAccessingOrigin, kAccessingOrigin},
               {InputKey::kTopFrameOrigin, kTopFrameOrigin}},
              {OutputKey::kIsSharedStorageAllowed, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsSharedStorageSelectURLAllowed) {
  url::Origin kAccessingOrigin =
      url::Origin::Create(GURL("https://storage.com"));
  url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top-frame.com"));
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsSharedStorageSelectURLAllowed(kTopFrameOrigin, kAccessingOrigin))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kAccessingOrigin, kAccessingOrigin},
               {InputKey::kTopFrameOrigin, kTopFrameOrigin}},
              {OutputKey::kIsSharedStorageSelectURLAllowed, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_IsPrivateAggregationAllowed) {
  url::Origin kAdMeasurementReportingOrigin =
      url::Origin::Create(GURL("https://reporting.com"));
  url::Origin kTopFrameOrigin =
      url::Origin::Create(GURL("https://top-frame.com"));
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsPrivateAggregationAllowed(kTopFrameOrigin,
                                          kAdMeasurementReportingOrigin))
      .WillOnce(testing::Return(true));

  CheckOutput(
      {{InputKey::kAdMeasurementReportingOrigin, kAdMeasurementReportingOrigin},
       {InputKey::kTopFrameOrigin, kTopFrameOrigin}},
      {OutputKey::kIsPrivateAggregationAllowed, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_TopicsConsentGiven) {
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsHasActiveConsent())
      .WillOnce(testing::Return(true));
  CheckOutput({}, {OutputKey::kTopicsConsentGiven, true});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_TopicsConsentLastUpdateReason) {
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsConsentLastUpdateSource())
      .WillOnce(testing::Return(
          privacy_sandbox::TopicsConsentUpdateSource::kSettings));
  CheckOutput({}, {OutputKey::kTopicsConsentLastUpdateReason,
                   privacy_sandbox::TopicsConsentUpdateSource::kSettings});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_TopicsConsentLastUpdateTime) {
  auto consent_time = base::Time::Now() - base::Hours(1);
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsConsentLastUpdateTime())
      .WillOnce(testing::Return(consent_time));
  CheckOutput({}, {OutputKey::kTopicsConsentLastUpdateTime, consent_time});
}

TEST_F(PrivacySandboxTestUtilTest, OutputKey_TopicsConsentStringIdentifiers) {
  auto identifier =
      IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL;
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsConsentLastUpdateText())
      .WillOnce(testing::Return(l10n_util::GetStringUTF8(identifier)));

  CheckOutput({}, {OutputKey::kTopicsConsentStringIdentifiers,
                   std::vector<int>{identifier}});
}

}  // namespace privacy_sandbox_test_util
