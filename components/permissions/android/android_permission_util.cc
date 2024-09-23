// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/android_permission_util.h"

#include "base/android/jni_array.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/core_jni/PermissionUtil_jni.h"
#include "components/permissions/android/jni_headers/AndroidPermissionRequester_jni.h"

namespace permissions {

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

bool AreAppLevelNotificationsEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PermissionUtil_areAppLevelNotificationsEnabled(env);
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

}  // namespace permissions
