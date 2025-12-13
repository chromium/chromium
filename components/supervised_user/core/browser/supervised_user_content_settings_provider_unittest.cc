// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_content_settings_provider.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/testing_pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace supervised_user {

class SupervisedUserProviderTest : public ::testing::Test {
 public:
  SupervisedUserProviderTest() = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  SupervisedUserSettingsService service_;
  scoped_refptr<TestingPrefStore> pref_store_;
  std::unique_ptr<SupervisedUserContentSettingsProvider> provider_;
  content_settings::MockObserver mock_observer_;
};

void SupervisedUserProviderTest::SetUp() {
  content_settings::ContentSettingsRegistry::GetInstance();
  pref_store_ = new TestingPrefStore();
  pref_store_->NotifyInitializationCompleted();
  service_.Init(pref_store_);
  service_.SetActive(true);
  provider_ =
      std::make_unique<SupervisedUserContentSettingsProvider>(&service_);
  provider_->AddObserver(&mock_observer_);
}

void SupervisedUserProviderTest::TearDown() {
  provider_->RemoveObserver(&mock_observer_);
  provider_->ShutdownOnUIThread();
  service_.Shutdown();
}

#if BUILDFLAG(IS_IOS)
// GEOLOCATION and GEOLOCATION_WITH_OPTIONS are not registered on IOS.
TEST_F(SupervisedUserProviderTest, GeolocationTest) {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION, false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default geolocation setting.
  service_.SetLocalSetting(kGeolocationDisabled, base::Value(true));

  // Check that nothing happened since the setting is not registered on IOS.
  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION, false);
  EXPECT_FALSE(rule_iterator);
}

TEST_F(SupervisedUserProviderTest, GeolocationWithOptionsTest) {
  base::test::ScopedFeatureList feature_list{
      content_settings::features::kApproximateGeolocationPermission};
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
                                 false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default geolocation setting.
  service_.SetLocalSetting(kGeolocationDisabled, base::Value(true));

  // Check that nothing happened since the setting is not registered on IOS.
  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS, false);
  EXPECT_FALSE(rule_iterator);
}

#else
TEST_F(SupervisedUserProviderTest, GeolocationTest) {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION, false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default geolocation setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::GEOLOCATION));
  service_.SetLocalSetting(kGeolocationDisabled, base::Value(true));

  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION, false);
  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));

  // Re-enable the default geolocation setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::GEOLOCATION));
  service_.SetLocalSetting(kGeolocationDisabled, base::Value(false));

  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION, false);
  EXPECT_FALSE(rule_iterator);
}

TEST_F(SupervisedUserProviderTest, GeolocationWithOptionsTest) {
  base::test::ScopedFeatureList feature_list{
      content_settings::features::kApproximateGeolocationPermission};
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
                                 false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default geolocation setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(
                  _, _, ContentSettingsType::GEOLOCATION_WITH_OPTIONS));
  service_.SetLocalSetting(kGeolocationDisabled, base::Value(true));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS, false);
  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  auto* info = content_settings::PermissionSettingsRegistry::GetInstance()->Get(
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
  GeolocationSetting expected_setting{PermissionOption::kDenied,
                                      PermissionOption::kDenied};
  EXPECT_EQ(PermissionSetting{expected_setting},
            content_settings::ValueToPermissionSetting(info, rule->value));

  // Re-enable the default geolocation setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(
                  _, _, ContentSettingsType::GEOLOCATION_WITH_OPTIONS));
  service_.SetLocalSetting(kGeolocationDisabled, base::Value(false));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS, false);
  EXPECT_FALSE(rule_iterator);
}
#endif  // !BUILDFLAG(IS_IOS)

TEST_F(SupervisedUserProviderTest, CookiesTest) {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::COOKIES, false);

  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            content_settings::ValueToContentSetting(rule->value));

  // Re-enable the default cookie setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::COOKIES));
  service_.SetLocalSetting(kCookiesAlwaysAllowed, base::Value(false));

  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::COOKIES, false);
  EXPECT_FALSE(rule_iterator);
}

TEST_F(SupervisedUserProviderTest, CameraMicTest) {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::MEDIASTREAM_CAMERA,
                                 false);
  EXPECT_FALSE(rule_iterator);
  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::MEDIASTREAM_MIC, false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default camera and microphone setting.
  EXPECT_CALL(
      mock_observer_,
      OnContentSettingChanged(_, _, ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_CALL(mock_observer_, OnContentSettingChanged(
                                  _, _, ContentSettingsType::MEDIASTREAM_MIC));
  service_.SetLocalSetting(kCameraMicDisabled, base::Value(true));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_CAMERA, false);
  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));

  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::MEDIASTREAM_MIC, false);
  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));

  // Re-enable the default camera and microphone setting.
  EXPECT_CALL(
      mock_observer_,
      OnContentSettingChanged(_, _, ContentSettingsType::MEDIASTREAM_CAMERA));
  EXPECT_CALL(mock_observer_, OnContentSettingChanged(
                                  _, _, ContentSettingsType::MEDIASTREAM_MIC));
  service_.SetLocalSetting(kCameraMicDisabled, base::Value(false));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_CAMERA, false);
  EXPECT_FALSE(rule_iterator);

  rule_iterator =
      provider_->GetRuleIterator(ContentSettingsType::MEDIASTREAM_MIC, false);
  EXPECT_FALSE(rule_iterator);
}

}  // namespace supervised_user
