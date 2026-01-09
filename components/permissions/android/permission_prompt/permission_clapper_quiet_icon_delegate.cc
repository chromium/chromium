// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_clapper_quiet_icon_delegate.h"

#include "base/android/jni_android.h"
#include "components/permissions/android/permission_prompt/permission_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace permissions {

PermissionClapperQuietIconDelegate::PermissionClapperQuietIconDelegate(
    content::WebContents* web_contents) {
  java_window_ = GetJavaWindow(web_contents);

  if (java_window_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    PermissionDialogController::ShowPermissionClapperQuietIcon(env,
                                                               java_window_);
  }
}

PermissionClapperQuietIconDelegate::~PermissionClapperQuietIconDelegate() {
  if (java_window_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    PermissionDialogController::DismissPermissionClapperQuietIcon(env,
                                                                  java_window_);
  }
}

base::android::ScopedJavaGlobalRef<jobject>
PermissionClapperQuietIconDelegate::GetJavaWindow(
    content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetTopLevelNativeWindow()) {
    return nullptr;
  }
  return base::android::ScopedJavaGlobalRef<jobject>(
      web_contents->GetTopLevelNativeWindow()->GetJavaObject());
}

}  // namespace permissions
