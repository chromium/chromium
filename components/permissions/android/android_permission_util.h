// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_ANDROID_PERMISSION_UTIL_H_
#define COMPONENTS_PERMISSIONS_ANDROID_ANDROID_PERMISSION_UTIL_H_

#include <string>
#include <vector>

#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace permissions {

// Appends to `out` the required Android permissions associated with
// `content_settings_type`.
void AppendRequiredAndroidPermissionsForContentSetting(
    ContentSettingsType content_settings_type,
    std::vector<std::string>* out);

// Appends to `out` the optional Android permissions associated with
// `content_settings_type`.
void AppendOptionalAndroidPermissionsForContentSetting(
    ContentSettingsType content_settings_type,
    std::vector<std::string>* out);

// Returns whether the required Android permission for `content_settings_type`
// are granted.
bool HasRequiredAndroidPermissionsForContentSetting(
    ui::WindowAndroid* window_android,
    ContentSettingsType content_settings_type);

// The states that indicate if the user should/can be re-nudged to accept
// permissions. In Chrome this correlates to the PermissionUpdateInfoBar.
enum class PermissionRepromptState {
  // No need to show the infobar as the permissions have been already granted.
  kNoNeed = 0,
  // Show the the permission infobar.
  kShow,
  // Can't show the permission infobar due to an internal state issue like
  // the WebContents or the AndroidWindow are not available.
  kCannotShow,
};

PermissionRepromptState ShouldRepromptUserForPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types);

// Filters the given |content_setting_types| to keep only types which are
// missing required Android permissions.
std::vector<ContentSettingsType>
GetContentSettingsWithMissingRequiredAndroidPermissions(
    const std::vector<ContentSettingsType>& content_settings_types,
    content::WebContents* web_contents);

// Appends to `out_required_permissions` the required Android permissions, and
// `out_optional_permissions` the optional Android permission, associated with
// each content setting type in given list.
void AppendRequiredAndOptionalAndroidPermissionsForContentSettings(
    const std::vector<ContentSettingsType>& content_settings_types,
    std::vector<std::string>& out_required_permissions,
    std::vector<std::string>& out_optional_permissions);

// Called to check whether Chrome settings and permissions allow requesting site
// level notification permission.
bool DoesAppLevelSettingsAllowSiteNotifications();

// Called to check whether Chrome has enabled app-level Notifications
// permission.
bool AreAppLevelNotificationsEnabled();

// Checks if Chrome needs Location permission for using Bluetooth.
bool NeedsLocationPermissionForBluetooth(content::WebContents* web_contents);

// Checks if Chrome needs Nearby Devices permission for using Bluetooth.
bool NeedsNearbyDevicesPermissionForBluetooth(
    content::WebContents* web_contents);

// Checks if Chrome needs Location Services to be turned on before using
// Bluetooth.
bool NeedsLocationServicesForBluetooth();

// Checks if Chrome can request system permissions for using Bluetooth.
bool CanRequestSystemPermissionsForBluetooth(
    content::WebContents* web_contents);

// Request the needed system permissions for using Bluetooth.
void RequestSystemPermissionsForBluetooth(content::WebContents* web_contents);

// Starts an activity for showing the Location Services setting page.
void RequestLocationServices(content::WebContents* web_contents);

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_ANDROID_PERMISSION_UTIL_H_
