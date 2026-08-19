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
  MOCK_METHOD(void, ForceChromeBuildForTests, (bool), (override, const));
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
        host_content_settings_map_.get(), &prefs_, false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        "chrome-extension");
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
        mock_privacy_sandbox_service(), mock_privacy_sandbox_settings(),
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
  auto user_rule_iterator =
      user_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                       /*off_the_record=*/false);

  EXPECT_TRUE(user_rule_iterator->HasNext());
  auto rule = user_rule_iterator->Next();
  EXPECT_EQ(rule->primary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(rule->secondary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(rule->value, base::Value(state));

  // Nothing should have ended up in the managed provider, which will present
  // as a null iterator.
  auto managed_rule_iterator =
      managed_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                          /*off_the_record=*/false);
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
  auto user_rule_iterator =
      user_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                       /*off_the_record=*/false);

  EXPECT_TRUE(user_rule_iterator->HasNext());
  auto rule = user_rule_iterator->Next();
  EXPECT_EQ(rule->primary_pattern.ToString(), kException);
  EXPECT_EQ(rule->secondary_pattern, ContentSettingsPattern::Wildcard());
  EXPECT_EQ(rule->value, base::Value(CONTENT_SETTING_BLOCK));

  // Nothing should have ended up in the managed provider, which will present
  // as a null iterator.
  auto managed_rule_iterator =
      managed_provider()->GetRuleIterator(ContentSettingsType::COOKIES,
                                          /*off_the_record=*/false);
  EXPECT_EQ(managed_rule_iterator, nullptr);
}

TEST_F(PrivacySandboxBaseTestUtilTest, VerifyAdvanceClockByStateKey) {
  base::Time start_time = base::Time::Now();
  ApplyTestState(StateKey::kAdvanceClockBy, base::Hours(1));
  EXPECT_EQ(start_time + base::Hours(1), base::Time::Now());
}

}  // namespace privacy_sandbox_test_util
