// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/geolocation_setting_delegate.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

using testing::Eq;
using testing::Field;
using testing::Pointee;

class ContentSettingsPrefProviderTest : public testing::Test {
 public:
  void SetUp() override {
    ContentSettingsRegistry::GetInstance();
    PrefProvider::RegisterProfilePrefs(prefs_.registry());
    provider_ = std::make_unique<PrefProvider>(&prefs_, false, true, false);
  }

  void TearDown() override { provider_->ShutdownOnUIThread(); }

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<PrefProvider> provider_;
};

TEST_F(ContentSettingsPrefProviderTest, EphemeralGrantClearsPersistentGrant) {
  auto* info = PermissionSettingsRegistry::GetInstance()->Get(
      ContentSettingsType::MEDIASTREAM_CAMERA);
  ASSERT_TRUE(info);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("https://example.com");
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();

  base::Value allowed_value = info->delegate().ToValue(CONTENT_SETTING_ALLOW);

  provider_->SetWebsiteSetting(primary_pattern, secondary_pattern,
                               ContentSettingsType::MEDIASTREAM_CAMERA,
                               allowed_value.Clone(),
                               ContentSettingConstraints());

  EXPECT_THAT(
      provider_->GetRule(primary_pattern.ToRepresentativeUrl(),
                         secondary_pattern.ToRepresentativeUrl(),
                         ContentSettingsType::MEDIASTREAM_CAMERA, false),
      Pointee(Field(&Rule::value, Eq(std::ref(allowed_value)))));

  ContentSettingConstraints constraints;
  constraints.set_session_model(mojom::SessionModel::ONE_TIME);
  constraints.set_ephemeral_clears_persistent_grant(true);

  provider_->SetWebsiteSetting(primary_pattern, secondary_pattern,
                               ContentSettingsType::MEDIASTREAM_CAMERA,
                               allowed_value.Clone(), constraints);

  EXPECT_EQ(provider_->GetRule(primary_pattern.ToRepresentativeUrl(),
                               secondary_pattern.ToRepresentativeUrl(),
                               ContentSettingsType::MEDIASTREAM_CAMERA, false),
            nullptr);
}

TEST_F(ContentSettingsPrefProviderTest, EphemeralGrantSetsBlockedToAsk) {
  auto* info = PermissionSettingsRegistry::GetInstance()->Get(
      ContentSettingsType::MEDIASTREAM_CAMERA);
  ASSERT_TRUE(info);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("https://example.com");
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();

  base::Value blocked_value = info->delegate().ToValue(CONTENT_SETTING_BLOCK);

  EXPECT_EQ(provider_->GetRule(primary_pattern.ToRepresentativeUrl(),
                               secondary_pattern.ToRepresentativeUrl(),
                               ContentSettingsType::MEDIASTREAM_CAMERA, false),
            nullptr);

  provider_->SetWebsiteSetting(primary_pattern, secondary_pattern,
                               ContentSettingsType::MEDIASTREAM_CAMERA,
                               blocked_value.Clone(),
                               ContentSettingConstraints());

  EXPECT_THAT(
      provider_->GetRule(primary_pattern.ToRepresentativeUrl(),
                         secondary_pattern.ToRepresentativeUrl(),
                         ContentSettingsType::MEDIASTREAM_CAMERA, false),
      Pointee(Field(&Rule::value, Eq(std::ref(blocked_value)))));

  ContentSettingConstraints constraints;
  constraints.set_session_model(mojom::SessionModel::ONE_TIME);

  base::Value allowed_value = info->delegate().ToValue(CONTENT_SETTING_ALLOW);

  provider_->SetWebsiteSetting(primary_pattern, secondary_pattern,
                               ContentSettingsType::MEDIASTREAM_CAMERA,
                               std::move(allowed_value), constraints);

  base::Value ask_value = info->delegate().ToValue(CONTENT_SETTING_ASK);

  EXPECT_EQ(provider_->GetRule(primary_pattern.ToRepresentativeUrl(),
                               secondary_pattern.ToRepresentativeUrl(),
                               ContentSettingsType::MEDIASTREAM_CAMERA, false),
            nullptr);
}

}  // namespace
}  // namespace content_settings
