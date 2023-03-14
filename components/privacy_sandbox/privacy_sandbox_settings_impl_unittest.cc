// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

#include "base/json/values_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace privacy_sandbox {

using Topic = browsing_topics::Topic;

namespace {

// C++20 introduces the "using enum" construct, which significantly reduces the
// required verbosity here. C++20 is support is coming to Chromium
// (crbug.com/1284275), with Mac / Windows / Linux support at the time of
// writing.
// TODO (crbug.com/1401686): Replace groups with commented lines when C++20 is
// supported.

// using enum privacy_sandbox_test_util::TestState;
using privacy_sandbox_test_util::StateKey;
constexpr auto kM1TopicsEnabledUserPrefValue =
    StateKey::kM1TopicsEnabledUserPrefValue;
constexpr auto kM1FledgeEnabledUserPrefValue =
    StateKey::kM1FledgeEnabledUserPrefValue;
constexpr auto kM1AdMeasurementEnabledUserPrefValue =
    StateKey::kM1AdMeasurementEnabledUserPrefValue;
constexpr auto kCookieControlsModeUserPrefValue =
    StateKey::kCookieControlsModeUserPrefValue;
constexpr auto kSiteDataUserDefault = StateKey::kSiteDataUserDefault;
constexpr auto kSiteDataUserExceptions = StateKey::kSiteDataUserExceptions;
constexpr auto kIsIncognito = StateKey::kIsIncognito;
constexpr auto kIsRestrictedAccount = StateKey::kIsRestrictedAccount;
constexpr auto kHasAppropriateTopicsConsent =
    StateKey::kHasAppropriateTopicsConsent;

// using enum privacy_sandbox_test_util::InputKey;
using privacy_sandbox_test_util::InputKey;
constexpr auto kTopFrameOrigin = InputKey::kTopFrameOrigin;
constexpr auto kTopicsURL = InputKey::kTopicsURL;
constexpr auto kFledgeAuctionPartyOrigin = InputKey::kFledgeAuctionPartyOrigin;
constexpr auto kAdMeasurementReportingOrigin =
    InputKey::kAdMeasurementReportingOrigin;
constexpr auto kAdMeasurementSourceOrigin =
    InputKey::kAdMeasurementSourceOrigin;
constexpr auto kAdMeasurementDestinationOrigin =
    InputKey::kAdMeasurementDestinationOrigin;
constexpr auto kAccessingOrigin = InputKey::kAccessingOrigin;

// using enum privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::OutputKey;
constexpr auto kIsTopicsAllowed = OutputKey::kIsTopicsAllowed;
constexpr auto kIsTopicsAllowedForContext =
    OutputKey::kIsTopicsAllowedForContext;
constexpr auto kIsFledgeAllowed = OutputKey::kIsFledgeAllowed;
constexpr auto kIsAttributionReportingAllowed =
    OutputKey::kIsAttributionReportingAllowed;
constexpr auto kMaySendAttributionReport = OutputKey::kMaySendAttributionReport;
constexpr auto kIsSharedStorageAllowed = OutputKey::kIsSharedStorageAllowed;
constexpr auto kIsSharedStorageSelectURLAllowed =
    OutputKey::kIsSharedStorageSelectURLAllowed;
constexpr auto kIsPrivateAggregationAllowed =
    OutputKey::kIsPrivateAggregationAllowed;

constexpr auto kIsTopicsAllowedMetric = OutputKey::kIsTopicsAllowedMetric;
constexpr auto kIsTopicsAllowedForContextMetric =
    OutputKey::kIsTopicsAllowedForContextMetric;
constexpr auto kIsFledgeAllowedMetric = OutputKey::kIsFledgeAllowedMetric;
constexpr auto kIsAttributionReportingAllowedMetric =
    OutputKey::kIsAttributionReportingAllowedMetric;
constexpr auto kMaySendAttributionReportMetric =
    OutputKey::kMaySendAttributionReportMetric;
constexpr auto kIsSharedStorageAllowedMetric =
    OutputKey::kIsSharedStorageAllowedMetric;
constexpr auto kIsSharedStorageSelectURLAllowedMetric =
    OutputKey::kIsSharedStorageSelectURLAllowedMetric;
constexpr auto kIsPrivateAggregationAllowedMetric =
    OutputKey::kIsPrivateAggregationAllowedMetric;
constexpr auto kIsAttributionReportingEverAllowed =
    OutputKey::kIsAttributionReportingEverAllowed;
constexpr auto kIsAttributionReportingEverAllowedMetric =
    OutputKey::kIsAttributionReportingEverAllowedMetric;

// using enum ContentSetting;
constexpr auto CONTENT_SETTING_ALLOW = ContentSetting::CONTENT_SETTING_ALLOW;
constexpr auto CONTENT_SETTING_BLOCK = ContentSetting::CONTENT_SETTING_BLOCK;

// using enum content_settings::CookieControlsMode;
constexpr auto kBlockThirdParty =
    content_settings::CookieControlsMode::kBlockThirdParty;

using privacy_sandbox_test_util::MultipleInputKeys;
using privacy_sandbox_test_util::MultipleOutputKeys;
using privacy_sandbox_test_util::MultipleStateKeys;
using privacy_sandbox_test_util::SiteDataExceptions;
using privacy_sandbox_test_util::TestCase;
using privacy_sandbox_test_util::TestInput;
using privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::TestState;

}  // namespace

class PrivacySandboxSettingsTest : public testing::Test {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    host_content_settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cookie_settings_ = new content_settings::CookieSettings(
        host_content_settings_map_.get(), &prefs_, false, "chrome-extension");
  }
  ~PrivacySandboxSettingsTest() override {
    host_content_settings_map()->ShutdownOnUIThread();
  }

  void SetUp() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate_ = mock_delegate.get();

    InitializePrefsBeforeStart();
    InitializeFeaturesBeforeStart();
    InitializeDelegateBeforeStart();

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettingsImpl>(
        std::move(mock_delegate), host_content_settings_map(), cookie_settings_,
        prefs());
  }

  virtual void InitializePrefsBeforeStart() {}

  virtual void InitializeFeaturesBeforeStart() {}

  virtual void InitializeDelegateBeforeStart() {
    mock_delegate()->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    mock_delegate()->SetUpIsIncognitoProfileResponse(/*incognito=*/false);
    mock_delegate()->SetUpHasAppropriateTopicsConsentResponse(
        /*has_appropriate_consent=*/true);
  }

  privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate*
  mock_delegate() {
    return mock_delegate_;
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }
  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }
  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &browser_task_environment_;
  }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return &mock_browsing_topics_service_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      mock_delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  std::unique_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;
};

TEST_F(PrivacySandboxSettingsTest, DefaultContentSettingBlockOverridePref) {
  // A block default content setting should override the Privacy Sandbox pref.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // An allow default or exception, whether via user or policy, should not
  // override the preference value.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*managed_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, CookieExceptionsApply) {
  // All cookie exceptions which disable access should apply to the Privacy
  // Sandbox. General topics calculations should however remain allowed.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://another-embedded.com", "*",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // The default managed content setting should apply, overriding any user ones,
  // and disabling Topics calculations.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Managed content setting exceptions should override both the privacy
  // sandbox pref and any user settings.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://another-embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://unrelated.com")),
      GURL("https://unrelated.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://unrelated-a.com")),
      url::Origin::Create(GURL("https://unrelated-b.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://unrelated-c.com")),
      url::Origin::Create(GURL("https://unrelated-d.com")),
      url::Origin::Create(GURL("https://unrelated-e.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://unrelated-a.com")),
      url::Origin::Create(GURL("https://unrelated-b.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // A less specific block exception should not override a more specific allow
  // exception. The effective content setting in this scenario is still allow,
  // even though a block exception exists.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://embedded.com", "https://another-test.com",
        ContentSetting::CONTENT_SETTING_ALLOW},
       {"https://[*.]embedded.com", "https://[*.]test.com",
        ContentSetting::CONTENT_SETTING_BLOCK},
       {"https://[*.]embedded.com", "https://[*.]another-test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Exceptions which specify a top frame origin should not match against other
  // top frame origins, or an empty origin.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_BLOCK}});

  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://top-level-origin.com")),
      GURL("https://embedded.com")));

  EXPECT_TRUE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://yet-another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Exceptions which specify a wildcard top frame origin should match both
  // empty top frames and non empty top frames.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://top-level-origin.com")),
      GURL("https://embedded.com")));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, ThirdPartyCookies) {
  // Privacy Sandbox APIs should be disabled if Third Party Cookies are blocked.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Privacy Sandbox APIs should be disabled if all cookies are blocked.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  // Privacy Sandbox APIs should be disabled if the privacy sandbox is disabled,
  // regardless of other cookie settings.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*managed_cookie_exceptions=*/
      {{"https://embedded.com", "https://test.com",
        ContentSetting::CONTENT_SETTING_ALLOW}});

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowed());
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(GURL("https://test.com")),
      GURL("https://embedded.com")));

  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->MaySendAttributionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsSharedStorageAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}

TEST_F(PrivacySandboxSettingsTest, IsPrivacySandboxEnabled) {
  // IsPrivacySandboxEnabled should directly reflect the state of the Privacy
  // Sandbox control. Prior to M1, this should also define whether Attribution
  // Reporting is ever allowed.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingEverAllowed());

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
  EXPECT_FALSE(privacy_sandbox_settings()->IsAttributionReportingEverAllowed());

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
  EXPECT_TRUE(privacy_sandbox_settings()->IsAttributionReportingEverAllowed());
}

TEST_F(PrivacySandboxSettingsTest, TopicsDataAccessibleSince) {
  ASSERT_NE(base::Time(), base::Time::Now());

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_settings()->OnCookiesCleared();

  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

TEST_F(PrivacySandboxSettingsTest, FledgeJoiningAllowed) {
  // Whether or not a site can join a user to an interest group is independent
  // of any other profile state.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/
      {{"https://example.com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/
      {{"https://example.com", "*", ContentSetting::CONTENT_SETTING_BLOCK}});
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  // Settings should match at the eTLD + 1 level.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", false);

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com:888"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com.au"))));

  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", true);

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com:888"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com.au"))));
}

TEST_F(PrivacySandboxSettingsTest, NonEtldPlusOneBlocked) {
  // Confirm that, as a fallback, hosts are accepted by SetFledgeJoiningAllowed.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("subsite.example.com",
                                                      false);

  // Applied setting should affect subdomaings.
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://another.subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  // When removing the setting, only an exact match, and not the associated
  // eTLD+1, should remove a setting.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", true);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://another.subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  privacy_sandbox_settings()->SetFledgeJoiningAllowed("subsite.example.com",
                                                      true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://another.subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  // IP addresses should also be accepted as a fallback.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("10.1.1.100", false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://10.1.1.100"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://10.1.1.100:8080"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://10.2.2.200"))));
}

TEST_F(PrivacySandboxSettingsTest, FledgeJoinSettingTimeRangeDeletion) {
  // Confirm that time range deletions work appropriately for FLEDGE join
  // settings.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("first.com", false);
  task_environment()->AdvanceClock(base::Hours(1));

  const base::Time kSecondSettingTime = base::Time::Now();
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("second.com", false);

  task_environment()->AdvanceClock(base::Hours(1));
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("third.com", false);

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://first.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://second.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://third.com"))));

  // Construct a deletion which only targets the second setting.
  privacy_sandbox_settings()->ClearFledgeJoiningAllowedSettings(
      kSecondSettingTime - base::Seconds(1),
      kSecondSettingTime + base::Seconds(1));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://first.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://second.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://third.com"))));

  // Perform a maximmal time range deletion, which should remove the two
  // remaining settings.
  privacy_sandbox_settings()->ClearFledgeJoiningAllowedSettings(
      base::Time(), base::Time::Max());
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://first.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://second.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://third.com"))));
}

TEST_F(PrivacySandboxSettingsTest, OnFirstPartySetsEnabledChanged) {
  // OnFirstPartySetsEnabledChanged() should only call observers when the
  // base::Feature is enabled and the pref changes.
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures({features::kFirstPartySets}, {});
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/true));

  prefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/false));
  prefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);

  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kFirstPartySets);
  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(testing::_)).Times(0);

  prefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled, true);
  prefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled, false);
}

TEST_F(PrivacySandboxSettingsTest, IsTopicAllowed) {
  // Confirm that allowing / blocking topics is correctly reflected by
  // IsTopicsAllowed().
  CanonicalTopic topic_one(Topic(1), CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic_two(Topic(2), CanonicalTopic::AVAILABLE_TAXONOMY);

  privacy_sandbox_settings()->SetTopicAllowed(topic_one, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));

  privacy_sandbox_settings()->SetTopicAllowed(topic_two, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));

  privacy_sandbox_settings()->SetTopicAllowed(topic_two, true);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));

  privacy_sandbox_settings()->SetTopicAllowed(topic_one, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));
}

TEST_F(PrivacySandboxSettingsTest, ClearingTopicSettings) {
  // Confirm that time range deletions affect the correct settings.
  CanonicalTopic topic_one(Topic(1), CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic_two(Topic(2), CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic topic_three(Topic(3), CanonicalTopic::AVAILABLE_TAXONOMY);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_three));

  privacy_sandbox_settings()->SetTopicAllowed(topic_one, false);
  task_environment()->AdvanceClock(base::Hours(1));

  const auto kSecondSettingTime = base::Time::Now();
  privacy_sandbox_settings()->SetTopicAllowed(topic_two, false);

  task_environment()->AdvanceClock(base::Hours(1));
  privacy_sandbox_settings()->SetTopicAllowed(topic_three, false);

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_three));

  // Construct a deletion which only targets the second setting.
  privacy_sandbox_settings()->ClearTopicSettings(
      kSecondSettingTime - base::Seconds(1),
      kSecondSettingTime + base::Seconds(1));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_three));

  // Perform a maximmal time range deletion, which should remove the two
  // remaining settings.
  privacy_sandbox_settings()->ClearTopicSettings(base::Time(),
                                                 base::Time::Max());
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_one));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_two));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_three));
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    prefs()->SetUserPref(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                         std::make_unique<base::Value>(::base::TimeToValue(
                             base::Time::FromTimeT(12345))));
  }
  void InitializeFeaturesBeforeStart() override {}
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff,
       UseLastTopicsDataAccessibleSince) {
  EXPECT_EQ(base::Time::FromTimeT(12345),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    host_content_settings_map()->SetDefaultContentSetting(
        ContentSettingsType::COOKIES,
        ContentSetting::CONTENT_SETTING_SESSION_ONLY);

    prefs()->SetUserPref(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                         std::make_unique<base::Value>(::base::TimeToValue(
                             base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn,
       UpdateTopicsDataAccessibleSince) {
  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

TEST_F(PrivacySandboxSettingsTest, DisabledInIncognito) {
  mock_delegate()->SetUpIsIncognitoProfileResponse(/*incognito=*/true);
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
}

class PrivacySandboxSettingsMockDelegateTest
    : public PrivacySandboxSettingsTest {
 public:
  void InitializeDelegateBeforeStart() override {
    // Do not set default handlers so each call must be mocked.
  }
};

TEST_F(PrivacySandboxSettingsMockDelegateTest, IsPrivacySandboxRestricted) {
  // When the sandbox is otherwise enabled, the delegate returning true for
  // IsPrivacySandboxRestricted() should disable the sandbox.
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  EXPECT_CALL(*mock_delegate(), IsPrivacySandboxRestricted())
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  EXPECT_CALL(*mock_delegate(), IsPrivacySandboxRestricted())
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  // The delegate should not override a disabled sandbox.
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_CALL(*mock_delegate(), IsPrivacySandboxRestricted())
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
}

class PrivacySandboxSettingLocalOverrideTest
    : public PrivacySandboxSettingsTest {
  void InitializeFeaturesBeforeStart() override {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting);
  }
};

TEST_F(PrivacySandboxSettingLocalOverrideTest, FollowsOverrideBehavior) {
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
}

// Tests class for the PrivacySandboxSettings4 / M1 launch.
class PrivacySandboxSettingsM1Test : public PrivacySandboxSettingsTest {
 public:
  void InitializeFeaturesBeforeStart() override {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxSettings4);
  }

 protected:
  using Status = PrivacySandboxSettingsImpl::Status;

  void RunTestCase(const TestState& test_state,
                   const TestInput& test_input,
                   const TestOutput& test_output) {
    ASSERT_FALSE(test_case_run_)
        << "Each test fixture should run a single test, to ensure the test "
           "profile is in a known state.";
    test_case_run_ = true;
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto* user_provider_raw = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    auto* managed_provider_raw = managed_provider.get();

    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        HostContentSettingsMap::PREF_PROVIDER);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        HostContentSettingsMap::POLICY_PROVIDER);

    privacy_sandbox_test_util::RunTestCase(
        task_environment(), prefs(), host_content_settings_map(),
        mock_delegate(), mock_browsing_topics_service(),
        privacy_sandbox_settings(), nullptr, user_provider_raw,
        managed_provider_raw, TestCase(test_state, test_input, test_output));
  }

 private:
  bool test_case_run_ = false;
};

TEST_F(PrivacySandboxSettingsM1Test, ApiPreferenceEnabled) {
  // Confirm that M1 kAPI respect M1 targeted preferences when enabled.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, ApiPreferenceDisabled) {
  // Confirm that M1 kAPI respect M1 targeted preferences when disabled.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageSelectURLAllowed, kIsPrivateAggregationAllowed},
           false},
          {kIsSharedStorageAllowed, true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kApisDisabled)},
          {kIsSharedStorageAllowedMetric, static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, CookieControlsModeHasNoEffect) {
  // Confirm that M1 kAPIs ignore the Cookie Controls mode preference.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kCookieControlsModeUserPrefValue, kBlockThirdParty}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataDefaultBlockExceptionApplies) {
  // Confirm that blocking site data for a site disables M1 kAPIs, with the
  // exception of the generic IsTopicsAllowed(). Topics should still be able
  // to calculate, as sites with ALLOW exceptions may still access site data.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kSiteDataUserDefault, CONTENT_SETTING_BLOCK}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed,
                              kIsAttributionReportingEverAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           false},
          {MultipleOutputKeys{kIsTopicsAllowedMetric,
                              kIsAttributionReportingEverAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kSiteDataAccessBlocked)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataBlockExceptionApplies) {
  // Confirm that blocking site data for a site disabled M1 kAPIs.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_ALLOW},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]embedded.com", CONTENT_SETTING_BLOCK}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed,
                              kIsAttributionReportingEverAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           false},
          {MultipleOutputKeys{kIsTopicsAllowedMetric,
                              kIsAttributionReportingEverAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kSiteDataAccessBlocked)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataAllowDoesntOverridePref) {
  // Confirm that allowing site data doesn't override preference values, even
  // via exceptions.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           false},
          {kSiteDataUserDefault, CONTENT_SETTING_ALLOW},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]embedded.com", CONTENT_SETTING_ALLOW},
                              {"[*.]top-frame.com", CONTENT_SETTING_ALLOW}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {kIsSharedStorageAllowed, true},
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageSelectURLAllowed, kIsPrivateAggregationAllowed},
           false},
          {kIsSharedStorageAllowedMetric, static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kApisDisabled)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataAllowExceptions) {
  // Confirm that site data exceptions override the default site data setting.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_BLOCK},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]embedded.com", CONTENT_SETTING_ALLOW}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, UnrelatedSiteDataBlock) {
  // Confirm that unrelated site data block exceptions don't affect kAPIs.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_ALLOW},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]unrelated.com", CONTENT_SETTING_BLOCK}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, UnrelatedSiteDataAllow) {
  // Confirm that unrelated site data allow exceptions don't affect kAPIs.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_BLOCK},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]unrelated.com", CONTENT_SETTING_ALLOW}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed,
                              kIsAttributionReportingEverAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           false},
          {MultipleOutputKeys{kIsTopicsAllowedMetric,
                              kIsAttributionReportingEverAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kSiteDataAccessBlocked)}});
}

TEST_F(PrivacySandboxSettingsM1Test, ApisAreOffInIncognito) {
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue,
                                   kIsIncognito},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kIncognitoProfile)}});
}

TEST_F(PrivacySandboxSettingsM1Test, ApisAreOffForRestrictedAccounts) {
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue,
                                   kIsRestrictedAccount},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext, kIsFledgeAllowed,
               kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kRestricted)}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckFledgeDependentApi_FledgeOn_OtherApiOn) {
  RunTestCase(TestState{{kM1FledgeEnabledUserPrefValue, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAccessingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsSharedStorageSelectURLAllowed, true}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckFledgeDependentApi_FledgeOff_OtherApiOff) {
  RunTestCase(TestState{{kM1FledgeEnabledUserPrefValue, false}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAccessingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsSharedStorageSelectURLAllowed, false}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckAdMeasurementDependentApi_AdMeasurementOn_OtherApiOn) {
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAdMeasurementReportingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsPrivateAggregationAllowed, true}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckAdMeasurementDependentApi_AdMeasurementOff_OtherApiOff) {
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, false}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAdMeasurementReportingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsPrivateAggregationAllowed, false}});
}

TEST_F(PrivacySandboxSettingsM1Test, NoAppropriateTopicsConsent) {
  // Confirm that when appropriate Topics consent is missing, Topics is disabled
  // while other APIs are unaffected.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kHasAppropriateTopicsConsent, false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsFledgeAllowed, kIsAttributionReportingAllowed,
               kIsAttributionReportingEverAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           true},
          {MultipleOutputKeys{kIsTopicsAllowed, kIsTopicsAllowedForContext},
           false},
          {MultipleOutputKeys{
               kIsFledgeAllowedMetric, kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric,
               kIsTopicsAllowedForContextMetric,
           },
           static_cast<int>(Status::kMismatchedConsent)}});
}

TEST_F(PrivacySandboxSettingsM1Test, TopicsConsentStatus) {
  // Confirm that if Topics is already disabled, and there is no appropriate
  // consent, the recorded status reflects that Topics is already disabled.
  RunTestCase(
      TestState{{kM1TopicsEnabledUserPrefValue, false},
                {kHasAppropriateTopicsConsent, false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
      },
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed, kIsTopicsAllowedForContext},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric,
               kIsTopicsAllowedForContextMetric,
           },
           static_cast<int>(Status::kApisDisabled)}});
}

}  // namespace privacy_sandbox
