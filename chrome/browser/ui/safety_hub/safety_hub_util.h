// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_UTIL_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_UTIL_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace safety_hub_util {

// Returns the amount of time to wait before cleaning up the list of revoked
// permissions.
base::TimeDelta GetCleanUpThreshold();

// Returns true if `url` belongs to a site with revoked abusive notifications.
bool IsUrlRevokedAbusiveNotification(HostContentSettingsMap* hcsm,
                                     const GURL& url);

// TODO(crbug/342210522): Make sure this function is tested in a following CL,
// when it is called in the `RevokedPermissionsService`. Returns true if
// `url` belongs to a site with revoked unused site permissions.
bool IsUrlRevokedUnusedSite(HostContentSettingsMap* hcsm, const GURL& url);

// Returns the list of all `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` settings,
// excluding settings that specify that they should be ignored.
ContentSettingsForOneType GetRevokedAbusiveNotificationPermissions(
    HostContentSettingsMap* hcsm);

// Get the dictionary setting value of the
// `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting. Returns `Type::NONE` if
// there is no `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS` setting value.
base::Value GetRevokedAbusiveNotificationPermissionsSettingValue(
    HostContentSettingsMap* hcsm,
    GURL setting_url);

// Returns true if there is a `REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS`
// setting value for the setting URL with the
// `safety_hub::kRevokedStatusDictKeyStr` key set to `safety_hub::kIgnoreStr`.
bool IsAbusiveNotificationRevocationIgnored(HostContentSettingsMap* hcsm,
                                            GURL setting_url);

#if !BUILDFLAG(IS_ANDROID)
// Fetches data for the version card to return data to the desktop UI.
base::Value::Dict GetVersionCardData();
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace safety_hub_util

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_UTIL_H_
