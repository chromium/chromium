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

constexpr char kAccessingOrigin[] = "https://storage.com";
constexpr char kTopFrameOrigin[] = "https://top-frame.com";

static url::Origin AccessingOrigin() {
  return url::Origin::Create(GURL(kAccessingOrigin));
}

static url::Origin TopFrameOrigin() {
  return url::Origin::Create(GURL(kTopFrameOrigin));
}

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
  MOCK_METHOD(int, GetRequiredPromptType, (int), (override, const));
  MOCK_METHOD(void, PromptActionOccurred, (int, int), (override, const));
};

}  // namespace

// TODO (crbug.com/1408187): Add coverage for all state / input / output keys.
class PrivacySandboxTestUtilTest {
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
        host_content_settings_map_.get(), &prefs_,
        /*tracking_protection_settings=*/nullptr, false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        /*tpcd_metadata_manager=*/nullptr, "chrome-extension");
  }

  ~PrivacySandboxTestUtilTest() {
    host_content_settings_map()->ShutdownOnUIThread();
  }

  void SetUpPrivacySandboxTest() {
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    user_provider_ = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    managed_provider_ = managed_provider.get();

    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        content_settings::ProviderType::kDefaultProvider);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        content_settings::ProviderType::kPolicyProvider);
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

class PrivacySandboxTestUtilBoolTest : public PrivacySandboxTestUtilTest,
                                       public testing::TestWithParam<bool> {
 public:
  ~PrivacySandboxTestUtilBoolTest() override = default;
  void SetUp() override { SetUpPrivacySandboxTest(); }
};

INSTANTIATE_TEST_SUITE_P(PrivacySandboxTestUtilBoolTestInstantiation,
                         PrivacySandboxTestUtilBoolTest,
                         testing::ValuesIn<bool>({false, true}));

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyM1TopicsEnabledStateKeySetsPref) {
  bool state = GetParam();
  ApplyTestState(StateKey::kM1TopicsEnabledUserPrefValue, state);
  EXPECT_EQ(
      prefs()->GetUserPref(prefs::kPrivacySandboxM1TopicsEnabled)->GetBool(),
      state);
}

TEST_P(PrivacySandboxTestUtilBoolTest,
       VerifykBlockAll3pcToggleEnabledStateKeySetsPref) {
  bool state = GetParam();
  ApplyTestState(StateKey::kBlockAll3pcToggleEnabledUserPrefValue, state);
  EXPECT_EQ(prefs()->GetUserPref(prefs::kBlockAll3pcToggleEnabled)->GetBool(),
            state);
}

TEST_P(PrivacySandboxTestUtilBoolTest,
       VerifykTrackingProtection3pcdEnabledStateKeySetsPref) {
  bool state = GetParam();
  ApplyTestState(StateKey::kTrackingProtection3pcdEnabledUserPrefValue, state);
  EXPECT_EQ(
      prefs()->GetUserPref(prefs::kTrackingProtection3pcdEnabled)->GetBool(),
      state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyM1FledgeEnabledStateKeySetsPref) {
  bool state = GetParam();
  ApplyTestState(StateKey::kM1FledgeEnabledUserPrefValue, state);
  EXPECT_EQ(
      prefs()->GetUserPref(prefs::kPrivacySandboxM1FledgeEnabled)->GetBool(),
      state);
}

TEST_P(PrivacySandboxTestUtilBoolTest,
       VerifyM1AdMeasurementEnabledStateKeySetsPref) {
  bool state = GetParam();
  ApplyTestState(StateKey::kM1AdMeasurementEnabledUserPrefValue, state);
  EXPECT_EQ(prefs()
                ->GetUserPref(prefs::kPrivacySandboxM1AdMeasurementEnabled)
                ->GetBool(),
            state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyIsIncognitoStateKey) {
  bool state = GetParam();
  ApplyTestState(StateKey::kIsIncognito, state);
  EXPECT_EQ(mock_delegate()->IsIncognitoProfile(), state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyIsRestrictedAccountStateKey) {
  bool state = GetParam();
  ApplyTestState(StateKey::kIsRestrictedAccount, state);
  EXPECT_EQ(mock_delegate()->IsPrivacySandboxRestricted(), state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyHasCurrentTopicsStateKey) {
  bool state = GetParam();
  ApplyTestState(StateKey::kHasCurrentTopics, state);
  EXPECT_EQ(
      mock_browsing_topics_service()->GetTopTopicsForDisplay().size() > 0u,
      state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyHasBlockedTopicsStateKey) {
  bool state = GetParam();
  testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              SetTopicAllowed(testing::_, false))
      .Times(state ? 1 : 0);
  ApplyTestState(StateKey::kHasBlockedTopics, state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyActiveTopicsConsentStateKey) {
  bool state = GetParam();
  ApplyTestState(StateKey::kActiveTopicsConsent, state);
  EXPECT_EQ(
      prefs()->GetUserPref(prefs::kPrivacySandboxTopicsConsentGiven)->GetBool(),
      state);
}

TEST_P(PrivacySandboxTestUtilBoolTest, VerifyTopicsToggleNewValueInputKey) {
  bool state = GetParam();
  testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsToggleChanged(state));
  ProvideInput(InputKey::kTopicsToggleNewValue, state);
}

class PrivacySandboxTestUtilCookieControlsModeTest
    : public PrivacySandboxTestUtilTest,
      public testing::TestWithParam<content_settings::CookieControlsMode> {
 public:
  ~PrivacySandboxTestUtilCookieControlsModeTest() override = default;
  void SetUp() override { SetUpPrivacySandboxTest(); }
};

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxTestUtilCookieControlsModeTestInstantiation,
    PrivacySandboxTestUtilCookieControlsModeTest,
    testing::ValuesIn<content_settings::CookieControlsMode>(
        {content_settings::CookieControlsMode::kBlockThirdParty,
         content_settings::CookieControlsMode::kIncognitoOnly,
         content_settings::CookieControlsMode::kOff}));

TEST_P(PrivacySandboxTestUtilCookieControlsModeTest,
       VerifyCookieControlsModeStateKeySetsPref) {
  content_settings::CookieControlsMode state = GetParam();
  ApplyTestState(StateKey::kCookieControlsModeUserPrefValue, state);
  EXPECT_EQ(static_cast<content_settings::CookieControlsMode>(
                prefs()->GetUserPref(prefs::kCookieControlsMode)->GetInt()),
            state);
}

class PrivacySandboxTestUtilContentSettingTest
    : public PrivacySandboxTestUtilTest,
      public testing::TestWithParam<ContentSetting> {
 public:
  ~PrivacySandboxTestUtilContentSettingTest() override = default;
  void SetUp() override { SetUpPrivacySandboxTest(); }
};

INSTANTIATE_TEST_SUITE_P(PrivacySandboxTestUtilContentSettingTestInstantiation,
                         PrivacySandboxTestUtilContentSettingTest,
                         testing::ValuesIn<ContentSetting>(
                             {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                              CONTENT_SETTING_SESSION_ONLY}));

TEST_P(PrivacySandboxTestUtilContentSettingTest,
       VerifySiteDataUserDefaultStateKey) {
  ContentSetting state = GetParam();
  ApplyTestState(StateKey::kSiteDataUserDefault, state);

  // The state should have ended up in the user provider we gave to the util.
  auto user_rule_iterator = user_provider()->GetRuleIterator(
      ContentSettingsType::COOKIES,
      /*incognito=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());

  EXPECT_TRUE(user_rule_iterator->HasNext());
  auto rule = user_rule_iterator->Next();
  EXPECT_EQ(rule->primary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(rule->secondary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(rule->value, base::Value(state));

  // Nothing should have ended up in the managed provider, which will present
  // as a null iterator.
  auto managed_rule_iterator = managed_provider()->GetRuleIterator(
      ContentSettingsType::COOKIES,
      /*incognito=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(managed_rule_iterator, nullptr);
}

class PrivacySandboxBaseTestUtilTest : public PrivacySandboxTestUtilTest,
                                       public testing::Test {
 public:
  ~PrivacySandboxBaseTestUtilTest() override = default;
  void SetUp() override { SetUpPrivacySandboxTest(); }
};

TEST_F(PrivacySandboxBaseTestUtilTest, VerifySiteDataUserExceptionStateKey) {
  const std::string kException = "https://embedded.com";
  ApplyTestState(StateKey::kSiteDataUserExceptions,
                 SiteDataExceptions{{kException, CONTENT_SETTING_BLOCK}});

  // The state should have ended up in the user provider we gave to the util.
  auto user_rule_iterator = user_provider()->GetRuleIterator(
      ContentSettingsType::COOKIES,
      /*incognito=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());

  EXPECT_TRUE(user_rule_iterator->HasNext());
  auto rule = user_rule_iterator->Next();
  EXPECT_EQ(rule->primary_pattern.ToString(), kException);
  EXPECT_EQ(rule->secondary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(rule->value, base::Value(CONTENT_SETTING_BLOCK));

  // Nothing should have ended up in the managed provider, which will present
  // as a null iterator.
  auto managed_rule_iterator = managed_provider()->GetRuleIterator(
      ContentSettingsType::COOKIES,
      /*incognito=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());
  EXPECT_EQ(managed_rule_iterator, nullptr);
}

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyAdvanceClockByStateKey) {
  base::Time start_time = base::Time::Now();
  ApplyTestState(StateKey::kAdvanceClockBy, base::Hours(1));
  EXPECT_EQ(start_time + base::Hours(1), base::Time::Now());
}

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyPromptActionOccurredInputKey) {
  constexpr int kArbitraryValue = 7;
  testing::Mock::VerifyAndClearExpectations(mock_privacy_sandbox_service());
  EXPECT_CALL(*mock_privacy_sandbox_service(),
              PromptActionOccurred(kArbitraryValue, /*kDesktop*/ 0));
  ProvideInput(InputKey::kPromptAction, kArbitraryValue);
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsTopicsAllowedForContextOutputKey) {
  GURL kTopicsURL = GURL("https://topics.com");

  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsTopicsAllowedForContext(TopFrameOrigin(), kTopicsURL, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kTopicsURL, kTopicsURL},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()}},
              {OutputKey::kIsTopicsAllowedForContext, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyIsTopicsAllowedOutputKey) {
  EXPECT_CALL(*mock_privacy_sandbox_settings(), IsTopicsAllowed())
      .WillOnce(testing::Return(true));
  CheckOutput({}, {OutputKey::kIsTopicsAllowed, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyIsFledgeAllowedOutputKey) {
  url::Origin kFledgeAuctionPartyOrigin =
      url::Origin::Create(GURL("https://fledge.com"));

  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsFledgeAllowed(TopFrameOrigin(), kFledgeAuctionPartyOrigin,
                      content::InterestGroupApiOperation::kJoin, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kFledgeAuctionPartyOrigin, kFledgeAuctionPartyOrigin},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()}},
              {OutputKey::kIsFledgeJoinAllowed, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsAttributionReportingAllowedOutputKey) {
  url::Origin kAdMeasurementReportingOrigin =
      url::Origin::Create(GURL("https://measurement.com"));

  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsAttributionReportingAllowed(
                  TopFrameOrigin(), kAdMeasurementReportingOrigin, nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput(
      {{InputKey::kAdMeasurementReportingOrigin, kAdMeasurementReportingOrigin},
       {InputKey::kTopFrameOrigin, TopFrameOrigin()}},
      {OutputKey::kIsAttributionReportingAllowed, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyMaySendAttributionReportOutputKey) {
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

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyIsSharedStorageAllowedOutputKey) {
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsSharedStorageAllowed(TopFrameOrigin(), AccessingOrigin(),
                             /*out_debug_message=*/nullptr,
                             /*console_frame=*/nullptr,
                             /*out_block_is_site_setting_specific=*/nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kAccessingOrigin, AccessingOrigin()},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()}},
              {OutputKey::kIsSharedStorageAllowed, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsSharedStorageSelectURLAllowedOutputKey) {
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsSharedStorageSelectURLAllowed(
                  TopFrameOrigin(), AccessingOrigin(),
                  /*out_debug_message=*/nullptr,
                  /*out_block_is_site_setting_specific=*/nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput({{InputKey::kAccessingOrigin, AccessingOrigin()},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()}},
              {OutputKey::kIsSharedStorageSelectURLAllowed, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsPrivateAggregationAllowedOutputKey) {
  url::Origin kAdMeasurementReportingOrigin =
      url::Origin::Create(GURL("https://reporting.com"));

  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsPrivateAggregationAllowed(
                  TopFrameOrigin(), kAdMeasurementReportingOrigin,
                  /*out_block_is_site_setting_specific=*/nullptr))
      .WillOnce(testing::Return(true));

  CheckOutput(
      {{InputKey::kAdMeasurementReportingOrigin, kAdMeasurementReportingOrigin},
       {InputKey::kTopFrameOrigin, TopFrameOrigin()}},
      {OutputKey::kIsPrivateAggregationAllowed, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyTopicsConsentGivenOutputKey) {
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsHasActiveConsent())
      .WillOnce(testing::Return(true));
  CheckOutput({}, {OutputKey::kTopicsConsentGiven, true});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyTopicsConsentLastUpdateReasonOutputKey) {
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsConsentLastUpdateSource())
      .WillOnce(testing::Return(
          privacy_sandbox::TopicsConsentUpdateSource::kSettings));
  CheckOutput({}, {OutputKey::kTopicsConsentLastUpdateReason,
                   privacy_sandbox::TopicsConsentUpdateSource::kSettings});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyTopicsConsentLastUpdateTimeOutputKey) {
  auto consent_time = base::Time::Now() - base::Hours(1);
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsConsentLastUpdateTime())
      .WillOnce(testing::Return(consent_time));
  CheckOutput({}, {OutputKey::kTopicsConsentLastUpdateTime, consent_time});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyTopicsConsentStringIdentifiersOutputKey) {
  auto identifier =
      IDS_SETTINGS_TOPICS_PAGE_CURRENT_TOPICS_DESCRIPTION_CANONICAL;
  EXPECT_CALL(*mock_privacy_sandbox_service(), TopicsConsentLastUpdateText())
      .WillOnce(testing::Return(l10n_util::GetStringUTF8(identifier)));

  CheckOutput({}, {OutputKey::kTopicsConsentStringIdentifiers,
                   std::vector<int>{identifier}});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsSharedStorageAllowedDebugMessageOutputKey) {
  std::string actual_out_debug_message;
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsSharedStorageAllowed(TopFrameOrigin(), AccessingOrigin(),
                             /*out_debug_message=*/&actual_out_debug_message,
                             /*console_frame=*/nullptr,
                             /*out_block_is_site_setting_specific=*/nullptr))
      .WillOnce(testing::Return(true));

  // The expected debug message is a non-null empty string here because we using
  // a mock method.
  std::string expected_out_debug_message;
  CheckOutput(
      {{InputKey::kAccessingOrigin, AccessingOrigin()},
       {InputKey::kTopFrameOrigin, TopFrameOrigin()},
       {InputKey::kOutSharedStorageDebugMessage, &actual_out_debug_message}},
      {OutputKey::kIsSharedStorageAllowedDebugMessage,
       &expected_out_debug_message});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsSharedStorageSelectURLAllowedDebugMessageOutputKey) {
  std::string actual_out_debug_message;
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsSharedStorageSelectURLAllowed(
                  TopFrameOrigin(), AccessingOrigin(),
                  /*out_debug_message=*/&actual_out_debug_message,
                  /*out_block_is_site_setting_specific=*/nullptr))
      .WillOnce(testing::Return(true));

  // The expected debug message is a non-null empty string here because we using
  // a mock method.
  std::string expected_out_debug_message;
  CheckOutput({{InputKey::kAccessingOrigin, AccessingOrigin()},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()},
               {InputKey::kOutSharedStorageSelectURLDebugMessage,
                &actual_out_debug_message}},
              {OutputKey::kIsSharedStorageSelectURLAllowedDebugMessage,
               &expected_out_debug_message});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsSharedStorageBlockSiteSettingSpecificOutputKey) {
  bool actual_out_block_is_site_setting_specific = true;
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsSharedStorageAllowed(TopFrameOrigin(), AccessingOrigin(),
                             /*out_debug_message=*/nullptr,
                             /*console_frame=*/nullptr,
                             /*out_block_is_site_setting_specific=*/
                             &actual_out_block_is_site_setting_specific))
      .WillOnce(testing::DoAll(testing::SetArgPointee<4>(false),
                               testing::Return(true)));

  // The expected value for `out_block_is_site_setting_specific` here is false
  // because we are using a mock method that sets it to false.
  bool expected_out_block_is_site_setting_specific = false;
  CheckOutput({{InputKey::kAccessingOrigin, AccessingOrigin()},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()},
               {InputKey::kOutSharedStorageBlockIsSiteSettingSpecific,
                &actual_out_block_is_site_setting_specific}},
              {OutputKey::kIsSharedStorageBlockSiteSettingSpecific,
               &expected_out_block_is_site_setting_specific});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsSharedStorageSelectURLBlockSiteSettingSpecificOutputKey) {
  bool actual_out_block_is_site_setting_specific = true;
  EXPECT_CALL(*mock_privacy_sandbox_settings(),
              IsSharedStorageSelectURLAllowed(
                  TopFrameOrigin(), AccessingOrigin(),
                  /*out_debug_message=*/nullptr,
                  /*out_block_is_site_setting_specific=*/
                  &actual_out_block_is_site_setting_specific))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(false),
                               testing::Return(true)));

  // The expected value for `out_block_is_site_setting_specific` here is false
  // because we are using a mock method that sets it to false.
  bool expected_out_block_is_site_setting_specific = false;
  CheckOutput({{InputKey::kAccessingOrigin, AccessingOrigin()},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()},
               {InputKey::kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
                &actual_out_block_is_site_setting_specific}},
              {OutputKey::kIsSharedStorageSelectURLBlockSiteSettingSpecific,
               &expected_out_block_is_site_setting_specific});
}

TEST_F(PrivacySandboxBaseTestUtilTest,
       VerifyIsPrivateAggregationBlockSiteSettingSpecificOutputKey) {
  bool actual_out_block_is_site_setting_specific = true;
  EXPECT_CALL(
      *mock_privacy_sandbox_settings(),
      IsPrivateAggregationAllowed(TopFrameOrigin(), AccessingOrigin(),
                                  /*out_block_is_site_setting_specific=*/
                                  &actual_out_block_is_site_setting_specific))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(false),
                               testing::Return(true)));

  // The expected value for `out_block_is_site_setting_specific` here is false
  // because we are using a mock method that sets it to false.
  bool expected_out_block_is_site_setting_specific = false;
  CheckOutput({{InputKey::kAccessingOrigin, AccessingOrigin()},
               {InputKey::kTopFrameOrigin, TopFrameOrigin()},
               {InputKey::kOutPrivateAggregationBlockIsSiteSettingSpecific,
                &actual_out_block_is_site_setting_specific}},
              {OutputKey::kIsPrivateAggregationBlockSiteSettingSpecific,
               &expected_out_block_is_site_setting_specific});
}

}  // namespace privacy_sandbox_test_util
