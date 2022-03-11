// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/android_permission_util.h"

#include "base/android/jni_array.h"
#include "components/permissions/android/jni_headers/AndroidPermissionRequester_jni.h"
#include "components/permissions/android/jni_headers/PermissionUtil_jni.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

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

bool AreAppLevelNotificationsEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PermissionUtil_areAppLevelNotificationsEnabled(env);
}

}  // namespace permissions
