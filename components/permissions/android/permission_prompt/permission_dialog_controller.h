// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_CONTROLLER_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_CONTROLLER_H_

#include <jni.h>

#include "base/android/jni_android.h"

namespace permissions {

// Wrapper class for the PermissionDialogController Java class.
// This class owns the JNI header inclusion to satisfy the JNI Zero
// single-inclusion rule.
class PermissionDialogController {
 public:
  static void CreateDialog(JNIEnv* env,
                           const base::android::JavaRef<jobject>& delegate);

  static void ShowPermissionClapperQuietIcon(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& window);

  static void DismissPermissionClapperQuietIcon(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& window);

  static void ShowLoudClapperDialogResultIcon(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& window,
      int content_setting);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_CONTROLLER_H_
