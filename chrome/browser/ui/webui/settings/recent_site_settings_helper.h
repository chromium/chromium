// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_RECENT_SITE_SETTINGS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_RECENT_SITE_SETTINGS_HELPER_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

class Profile;

namespace site_settings {

struct TimestampedSetting {
  base::Time timestamp;
  ContentSettingsType content_type;
  ContentSetting content_setting;
  site_settings::SiteSettingSource setting_source;

  TimestampedSetting();
  TimestampedSetting(const TimestampedSetting& other);
  TimestampedSetting& operator=(const TimestampedSetting& other) = default;
  TimestampedSetting(TimestampedSetting&& other) = default;
  TimestampedSetting(base::Time timestamp,
                     ContentSettingsType content_type,
                     ContentSetting content_setting,
                     site_settings::SiteSettingSource setting_source);
  ~TimestampedSetting();
};

struct RecentSitePermissions {
  GURL origin;
  bool incognito;
  std::vector<TimestampedSetting> settings;

  RecentSitePermissions();
  RecentSitePermissions(const RecentSitePermissions& other);
  RecentSitePermissions& operator=(const RecentSitePermissions& other) =
      default;
  RecentSitePermissions(RecentSitePermissions&& other);
  RecentSitePermissions(GURL origin,
                        bool incognito,
                        std::vector<TimestampedSetting> settings);
  ~RecentSitePermissions();
};

// Returns a list containing the most recent permission changes for the
// provided content types grouped by origin/profile (incognito, regular)
// combinations. Limited to |max_sources| origin/profile pairings and ordered
// from most recently adjusted site to least recently. Includes permissions
// changed by embargo, but not those changed by enterprise policy.
std::vector<RecentSitePermissions> GetRecentSitePermissions(
    Profile* profile,
    std::vector<ContentSettingsType> content_types,
    size_t max_sources);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_RECENT_SITE_SETTINGS_HELPER_H_
