// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

namespace web_app {

using RemoveIsolatedWebAppDataTest = WebAppTest;

TEST_F(RemoveIsolatedWebAppDataTest, ResetAllContentSettings) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  GURL url("isolated-app://abcdef");
  host_content_settings_map->SetPermissionSettingDefaultScope(
      url, url, ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
      GeolocationSetting{.approximate = PermissionOption::kAllowed,
                         .precise = PermissionOption::kDenied});
  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::NOTIFICATIONS,
      ContentSetting::CONTENT_SETTING_ALLOW);

  internal::ResetAllContentSettingsForIsolatedWebApp(profile(), url);

  EXPECT_EQ(host_content_settings_map->GetPermissionSetting(
                url, url, ContentSettingsType::GEOLOCATION_WITH_OPTIONS),
            content_settings::PermissionSettingsRegistry::GetInstance()
                ->Get(ContentSettingsType::GEOLOCATION_WITH_OPTIONS)
                ->GetInitialDefaultSetting());
  EXPECT_EQ(host_content_settings_map->GetPermissionSetting(
                url, url, ContentSettingsType::NOTIFICATIONS),
            content_settings::PermissionSettingsRegistry::GetInstance()
                ->Get(ContentSettingsType::NOTIFICATIONS)
                ->GetInitialDefaultSetting());
}

}  // namespace web_app
