// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/menu_notification_service.h"

#include <memory>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/safety_hub/menu_notification.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/permissions/constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
void CreateMockUnusedSitePermissionsEntry(
    HostContentSettingsMap* hcsm,
    UnusedSitePermissionsService* service) {
  // Revoke permission and update the unused site permission service.
  const std::string url1 = "https://example1.com:443";
  auto dict = base::Value::Dict().Set(
      permissions::kRevokedKey, base::Value::List().Append(static_cast<int32_t>(
                                    ContentSettingsType::GEOLOCATION)));
  hcsm->SetWebsiteSettingDefaultScope(
      GURL(url1), GURL(url1),
      ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS,
      base::Value(dict.Clone()));
  safety_hub_test_util::UpdateSafetyHubServiceAsync(service);
}
}  // namespace

class SafetyHubMenuNotificationServiceTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    RegisterSafetyHubProfilePrefs(prefs_.registry());
    hcsm_ = base::MakeRefCounted<HostContentSettingsMap>(&prefs_, false, true,
                                                         false, false);
    unused_site_permissions_service_ =
        std::make_unique<UnusedSitePermissionsService>(hcsm_.get(), &prefs_);
    menu_notification_service_ =
        std::make_unique<SafetyHubMenuNotificationService>(
            &prefs_, unused_site_permissions_service_.get());
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
  PrefService* prefs() { return &prefs_; }

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
  CreateMockUnusedSitePermissionsEntry(hcsm(),
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

TEST_F(SafetyHubMenuNotificationServiceTest, PersistInPrefs) {
  // Creating a mock result, which should result in a notification to be
  // available.
  CreateMockUnusedSitePermissionsEntry(hcsm(),
                                       unused_site_permissions_service());

  absl::optional<std::pair<int, std::u16string>> notification =
      menu_notification_service()->GetNotificationToShow();
  EXPECT_TRUE(notification.has_value());
  SafetyHubMenuNotification* old_notification =
      menu_notification_service()->GetNotificationForTesting(
          SafetyHubServiceType::UNUSED_SITE_PERMISSIONS);
  EXPECT_TRUE(old_notification->IsCurrentlyActive());
  auto* old_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          old_notification->GetResultForTesting());
  EXPECT_EQ(1U, old_result->GetRevokedPermissions().size());

  // After |GetNotificationToShow()| was called, the notification should be
  // persisted in the prefs. When creating a new service, that result should be
  // loaded in memory.
  std::unique_ptr<SafetyHubMenuNotificationService> new_service =
      std::make_unique<SafetyHubMenuNotificationService>(
          prefs(), unused_site_permissions_service());
  // Getting the in-memory notification to prevent the service from generating a
  // new one.
  SafetyHubMenuNotification* new_notification =
      new_service->GetNotificationForTesting(
          SafetyHubServiceType::UNUSED_SITE_PERMISSIONS);
  EXPECT_TRUE(new_notification->IsCurrentlyActive());
  EXPECT_EQ(
      l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_HUB_UNUSED_SITE_PERMISSIONS_MENU_NOTIFICATION, 1),
      new_notification->GetNotificationString());
  auto* new_result =
      static_cast<UnusedSitePermissionsService::UnusedSitePermissionsResult*>(
          new_notification->GetResultForTesting());

  EXPECT_EQ(old_result->GetRevokedPermissions().size(),
            new_result->GetRevokedPermissions().size());
  EXPECT_EQ(old_result->GetRevokedPermissions().front().origin,
            new_result->GetRevokedPermissions().front().origin);
  EXPECT_EQ(old_result->GetRevokedPermissions().front().expiration,
            new_result->GetRevokedPermissions().front().expiration);
  EXPECT_EQ(old_result->GetRevokedPermissions().front().permission_types,
            new_result->GetRevokedPermissions().front().permission_types);
}
