// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings.h"

#include "base/json/values_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace privacy_sandbox {

using Topic = browsing_topics::Topic;

class PrivacySandboxSettingsTest : public testing::TestWithParam<bool> {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());

    host_content_settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */);
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

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettings>(
        std::move(mock_delegate), host_content_settings_map(), cookie_settings_,
        prefs(), IsIncognitoProfile());
  }

  virtual void InitializePrefsBeforeStart() {}

  virtual void InitializeFeaturesBeforeStart() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          privacy_sandbox::kPrivacySandboxSettings3);
    } else {
      feature_list_.InitAndDisableFeature(
          privacy_sandbox::kPrivacySandboxSettings3);
    }
  }

  virtual void InitializeDelegateBeforeStart() {
    mock_delegate()->SetupDefaultResponse(/*restricted=*/false);
  }

  virtual bool IsIncognitoProfile() { return false; }

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

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>
      mock_delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  std::unique_ptr<PrivacySandboxSettings> privacy_sandbox_settings_;
};

TEST_P(PrivacySandboxSettingsTest, DefaultContentSettingBlockOverridePref) {
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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));
}

TEST_P(PrivacySandboxSettingsTest, CookieExceptionsApply) {
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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      GURL("https://unrelated.com"),
      url::Origin::Create(GURL("https://unrelated.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://unrelated-a.com")),
      url::Origin::Create(GURL("https://unrelated-b.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://unrelated-c.com")),
      url::Origin::Create(GURL("https://unrelated-d.com")),
      url::Origin::Create(GURL("https://unrelated-e.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{GURL("https://another-embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

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
      GURL("https://embedded.com"), absl::nullopt));

  EXPECT_TRUE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_TRUE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://yet-another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_TRUE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{GURL("https://another-embedded.com/")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"), absl::nullopt));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{GURL("https://another-embedded.com")},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));
}

TEST_P(PrivacySandboxSettingsTest, ThirdPartyCookies) {
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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));

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
      GURL("https://embedded.com"),
      url::Origin::Create(GURL("https://test.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsConversionMeasurementAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_FALSE(privacy_sandbox_settings()->ShouldSendConversionReport(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://another-test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));

  EXPECT_FALSE(privacy_sandbox_settings()->IsFledgeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
  EXPECT_EQ(std::vector<GURL>{},
            privacy_sandbox_settings()->FilterFledgeAllowedParties(
                url::Origin::Create(GURL("https://test.com")),
                {GURL("https://embedded.com"),
                 GURL("https://another-embedded.com")}));
}

TEST_P(PrivacySandboxSettingsTest, IsPrivacySandboxEnabled) {
  // IsPrivacySandboxEnabled should directly reflect the state of the Privacy
  // Sandbox control.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});
  EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
}

TEST_P(PrivacySandboxSettingsTest, TopicsDataAccessibleSince) {
  ASSERT_NE(base::Time(), base::Time::Now());

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_settings()->OnCookiesCleared();

  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

TEST_P(PrivacySandboxSettingsTest, FledgeJoiningAllowed) {
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

TEST_P(PrivacySandboxSettingsTest, NonEtldPlusOneBlocked) {
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

TEST_P(PrivacySandboxSettingsTest, FledgeJoinSettingTimeRangeDeletion) {
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

TEST_P(PrivacySandboxSettingsTest, TrustTokensAllowed) {
  // IsTrustTokensAllowed() should follow the top level privacy sandbox setting
  // as long as the release 3 feature is enabled, always returning true
  // otherwise
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(privacy_sandbox::kPrivacySandboxSettings3);
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTrustTokenBlockingChanged(/*blocked=*/true));

  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTrustTokensAllowed());
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnTrustTokenBlockingChanged(/*blocked=*/false));
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTrustTokensAllowed());
  testing::Mock::VerifyAndClearExpectations(&observer);

  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(
      privacy_sandbox::kPrivacySandboxSettings3);
  EXPECT_CALL(observer, OnTrustTokenBlockingChanged(testing::_)).Times(0);

  privacy_sandbox_settings()->SetPrivacySandboxEnabled(false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTrustTokensAllowed());

  privacy_sandbox_settings()->SetPrivacySandboxEnabled(true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTrustTokensAllowed());
}

TEST_P(PrivacySandboxSettingsTest, IsTopicAllowed) {
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

TEST_P(PrivacySandboxSettingsTest, ClearingTopicSettings) {
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

INSTANTIATE_TEST_SUITE_P(PrivacySandboxSettingsTestInstance,
                         PrivacySandboxSettingsTest,
                         testing::Bool());

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

TEST_P(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff,
       UseLastTopicsDataAccessibleSince) {
  EXPECT_EQ(base::Time::FromTimeT(12345),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxSettingsTestCookiesClearOnExitTurnedOffInstance,
    PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff,
    testing::Bool());

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

TEST_P(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn,
       UpdateTopicsDataAccessibleSince) {
  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxSettingsTestCookiesClearOnExitTurnedOnInstance,
    PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn,
    testing::Bool());

class PrivacySandboxSettingsIncognitoTest : public PrivacySandboxSettingsTest {
  bool IsIncognitoProfile() override { return true; }
};

TEST_P(PrivacySandboxSettingsIncognitoTest, DisabledInIncognito) {
  // When the Release 3 flag is enabled, APIs should always be disabled in
  // incognito. The Release 3 flag is set based on the test param.
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(true);
  if (GetParam())
    EXPECT_FALSE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
  else
    EXPECT_TRUE(privacy_sandbox_settings()->IsPrivacySandboxEnabled());
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxSettingsIncognitoTestInstance,
                         PrivacySandboxSettingsIncognitoTest,
                         testing::Bool());

class PrivacySandboxSettingsMockDelegateTest
    : public PrivacySandboxSettingsTest {
 public:
  void InitializeDelegateBeforeStart() override {
    // Do not set default handlers so each call must be mocked.
  }
};

TEST_P(PrivacySandboxSettingsMockDelegateTest, IsPrivacySandboxRestricted) {
  // When the sandbox is otherwise enabled, the delegate returning true for
  // IsPrivacySandboxRestricted() should disable the sandbox.
  privacy_sandbox_settings()->SetPrivacySandboxEnabled(true);
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

INSTANTIATE_TEST_SUITE_P(PrivacySandboxSettingsMockDelegateTestInstance,
                         PrivacySandboxSettingsMockDelegateTest,
                         testing::Bool());

}  // namespace privacy_sandbox
