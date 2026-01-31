// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_CLAPPER_QUIET_ICON_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_CLAPPER_QUIET_ICON_DELEGATE_H_

#include "base/android/jni_android.h"

namespace content {
class WebContents;
}

namespace permissions {

// Delegate class for PermissionClapperQuietIcon.
// This class manages the lifecycle of the Java-side Omnibox icon.
// It shows the icon on construction and dismisses it on destruction.
class PermissionClapperQuietIconDelegate {
 public:
  explicit PermissionClapperQuietIconDelegate(
      content::WebContents* web_contents);
  ~PermissionClapperQuietIconDelegate();

  PermissionClapperQuietIconDelegate(
      const PermissionClapperQuietIconDelegate&) = delete;
  PermissionClapperQuietIconDelegate& operator=(
      const PermissionClapperQuietIconDelegate&) = delete;

 private:
  base::android::ScopedJavaGlobalRef<jobject> GetJavaWindow(
      content::WebContents* web_contents);
  base::android::ScopedJavaGlobalRef<jobject> java_window_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_CLAPPER_QUIET_ICON_DELEGATE_H_
