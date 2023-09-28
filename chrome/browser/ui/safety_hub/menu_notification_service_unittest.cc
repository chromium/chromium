// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <memory>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/permissions/constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

class SafetyHubMenuNotificationServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    hcsm_ = base::MakeRefCounted<HostContentSettingsMap>(&prefs_, false, true,
                                                         false, false);
    unused_site_permissions_service_ =
        std::make_unique<UnusedSitePermissionsService>(hcsm_.get(), &prefs_);
    menu_notification_service_ =
        std::make_unique<SafetyHubMenuNotificationService>(
            unused_site_permissions_service_.get());
  }

  void TearDown() override {
    hcsm_->ShutdownOnUIThread();
    base::RunLoop().RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  UnusedSitePermissionsService* unused_site_permissions_service() {
    return unused_site_permissions_service_.get();
  }
  SafetyHubMenuNotificationService* menu_notification_service() {
    return menu_notification_service_.get();
  }
  HostContentSettingsMap* hcsm() { return hcsm_.get(); }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> hcsm_;
  std::unique_ptr<UnusedSitePermissionsService>
      unused_site_permissions_service_;
  std::unique_ptr<SafetyHubMenuNotificationService> menu_notification_service_;
};

TEST_F(SafetyHubMenuNotificationServiceTest, GetNotificationToShowNoResult) {
  absl::optional<std::pair<int, std::u16string>> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_FALSE(notification.has_value());
}

TEST_F(SafetyHubMenuNotificationServiceTest, SingleNotificationToShow) {
  // Revoke permission and update the unused site permission service.
  const std::string url1 = "https://example1.com:443";
  auto dict = base::Value::Dict().Set(
      permissions::kRevokedKey, base::Value::List().Append(static_cast<int32_t>(
                                    ContentSettingsType::GEOLOCATION)));
  hcsm()->SetWebsiteSettingDefaultScope(
      GURL(url1), GURL(url1),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));
  safety_hub_test_util::UpdateSafetyHubServiceAsync(
      unused_site_permissions_service());

  // The notification to show should be the unused site permissions one with one
  // revoked permission. The relevant command should be to open Safety Hub.
  absl::optional<std::pair<int, std::u16string>> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  EXPECT_EQ(
      l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1),
      notification.value().second);
  EXPECT_EQ(IDC_OPEN_SAFETY_HUB, notification.value().first);
}
