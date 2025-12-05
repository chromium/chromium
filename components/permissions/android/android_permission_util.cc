// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/android_permission_util.h"

#include "base/android/jni_array.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/location/android/location_settings_impl.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/jni_headers/AndroidPermissionRequester_jni.h"
#include "components/permissions/android/jni_headers/PermissionUtil_jni.h"

namespace permissions {

namespace {

bool g_is_system_location_setting_enabled_for_test = false;

// Returns whether the Android location setting is enabled/disabled.
bool IsSystemLocationSettingEnabled() {
  if (g_is_system_location_setting_enabled_for_test) {
    return true;
  }
  LocationSettingsImpl location_settings;
  return location_settings.IsSystemLocationSettingEnabled();
}

}  // namespace

void AppendRequiredAndroidPermissionsForContentSetting(
    ContentSettingsType content_settings_type,
    std::vector<std::string>* out) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env,
      Java_PermissionUtil_getRequiredAndroidPermissionsForContentSetting(
          env, static_cast<int>(content_settings_type)),
      out);
}

void AppendOptionalAndroidPermissionsForContentSetting(
    ContentSettingsType content_settings_type,
    std::vector<std::string>* out) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env,
      Java_PermissionUtil_getOptionalAndroidPermissionsForContentSetting(
          env, static_cast<int>(content_settings_type)),
      out);
}

bool HasRequiredAndroidPermissionsForContentSetting(
    ui::WindowAndroid* window_android,
    ContentSettingsType content_settings_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AndroidPermissionRequester_hasRequiredAndroidPermissionsForContentSetting(
      env, window_android->GetJavaObject(),
      static_cast<int>(content_settings_type));
}

PermissionRepromptState ShouldRepromptUserForPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types) {
  if (!web_contents)
    return PermissionRepromptState::kCannotShow;

  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  if (!window_android)
    return PermissionRepromptState::kCannotShow;

  for (ContentSettingsType content_settings_type : content_settings_types) {
    if (!HasRequiredAndroidPermissionsForContentSetting(
            window_android, content_settings_type)) {
      PermissionUmaUtil::RecordMissingPermissionInfobarShouldShow(
          true, content_settings_types);
      return PermissionRepromptState::kShow;
    }
  }

  permissions::PermissionUmaUtil::RecordMissingPermissionInfobarShouldShow(
      false, content_settings_types);
  return PermissionRepromptState::kNoNeed;
}

std::vector<ContentSettingsType>
GetContentSettingsWithMissingRequiredAndroidPermissions(
    const std::vector<ContentSettingsType>& content_settings_types,
    content::WebContents* web_contents) {
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);

  std::vector<ContentSettingsType> filtered_types;
  for (ContentSettingsType content_settings_type : content_settings_types) {
    if (HasRequiredAndroidPermissionsForContentSetting(window_android,
                                                       content_settings_type)) {
      continue;
    }
    filtered_types.push_back(content_settings_type);
  }

  return filtered_types;
}

void AppendRequiredAndOptionalAndroidPermissionsForContentSettings(
    const std::vector<ContentSettingsType>& content_settings_types,
    std::vector<std::string>& out_required_permissions,
    std::vector<std::string>& out_optional_permissions) {
  for (ContentSettingsType content_settings_type : content_settings_types) {
    permissions::AppendRequiredAndroidPermissionsForContentSetting(
        content_settings_type, &out_required_permissions);
    permissions::AppendOptionalAndroidPermissionsForContentSetting(
        content_settings_type, &out_optional_permissions);
  }
}

bool DoesAppLevelSettingsAllowSiteNotifications() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PermissionUtil_doesAppLevelSettingsAllowSiteNotifications(env);
}

bool NeedsLocationPermissionForBluetooth(content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);
  return Java_PermissionUtil_needsLocationPermissionForBluetooth(
      env, window_android->GetJavaObject());
}

bool NeedsNearbyDevicesPermissionForBluetooth(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);
  return Java_PermissionUtil_needsNearbyDevicesPermissionForBluetooth(
      env, window_android->GetJavaObject());
}

bool NeedsLocationServicesForBluetooth() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PermissionUtil_needsLocationServicesForBluetooth(env);
}

bool CanRequestSystemPermissionsForBluetooth(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);
  return Java_PermissionUtil_canRequestSystemPermissionsForBluetooth(
      env, window_android->GetJavaObject());
}

bool HasSystemPermission(ContentSettingsType type,
                         content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetNativeView()) {
    return false;
  }
  if ((type == permissions::PermissionUtil::GetGeolocationType()) &&
      !IsSystemLocationSettingEnabled()) {
    return false;
  }
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);

  return HasRequiredAndroidPermissionsForContentSetting(window_android, type);
}

bool CanRequestSystemPermission(ContentSettingsType type,
                                content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetNativeView()) {
    return false;
  }
  if ((type == permissions::PermissionUtil::GetGeolocationType()) &&
      !IsSystemLocationSettingEnabled()) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);
  return Java_PermissionUtil_canRequestSystemPermission(
      env, static_cast<int>(type), window_android->GetJavaObject());
}

void RequestSystemPermissionsForBluetooth(content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);
  // TODO(crbug.com/40255210): Pass the callback from native layer.
  return Java_PermissionUtil_requestSystemPermissionsForBluetooth(
      env, window_android->GetJavaObject(), nullptr);
}

void RequestLocationServices(content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  DCHECK(window_android);
  return Java_PermissionUtil_requestLocationServices(
      env, window_android->GetJavaObject());
}

base::AutoReset<bool> EnableSystemLocationSettingForTesting() {
  return base::AutoReset<bool>(&g_is_system_location_setting_enabled_for_test,
                               true);
}

}  // namespace permissions

static void JNI_PermissionUtil_ResolvePermissionRequest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jint content_settings_type,
    jint content_setting) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  ContentSetting setting = static_cast<ContentSetting>(content_setting);

  if (!permission_request_manager) {
    return;
  }
  if (permission_request_manager->IsRequestInProgress() &&
      permission_request_manager->Requests().size() > 0 &&
      permission_request_manager->Requests()[0]->GetContentSettingsType() ==
          static_cast<ContentSettingsType>(content_settings_type)) {
    if (setting == CONTENT_SETTING_ALLOW) {
      base::UmaHistogramBoolean("Permissions.ClapperLoud.PageInfo.Subscribed",
                                true);
      permission_request_manager->Accept();
    } else if (setting == CONTENT_SETTING_BLOCK) {
      // There are multiple ways to deny the permission request. This histogram
      // will track the number of times the user denied the permission request
      // by closing the PageInfo.
      base::UmaHistogramBoolean("Permissions.ClapperLoud.PageInfo.Closed",
                                true);
      permission_request_manager->Deny();
    } else if (setting == CONTENT_SETTING_DEFAULT) {
      base::UmaHistogramBoolean("Permissions.ClapperLoud.PageInfo.Reset", true);
      // After the user interacts with the reset permission button in PageInfo,
      // all previously decided permissions are reset by setting them to
      // DEFAULT. There is no a default action or a state for permission
      // requests, so we need to explicitly dismiss the request.
      permission_request_manager->Dismiss();
    } else {
      // Currently, only ALLOW and BLOCK are supported. In case other actions
      // are added in the future, this should be updated.
      NOTREACHED();
    }
  }
}

DEFINE_JNI(PermissionUtil)
DEFINE_JNI(AndroidPermissionRequester)
