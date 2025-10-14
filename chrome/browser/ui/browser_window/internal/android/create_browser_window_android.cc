// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBrowserWindowCreateParamsImpl_jni.h"
#include "chrome/browser/ui/browser_window/internal/jni/BrowserWindowCreatorBridge_jni.h"

BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const gfx::Rect& bounds = create_params.initial_bounds;

  base::android::ScopedJavaLocalRef<jobject> j_create_params =
      Java_AndroidBrowserWindowCreateParamsImpl_create(
          env, static_cast<int>(create_params.type),
          create_params.profile->GetJavaObject(), bounds.x(), bounds.y(),
          bounds.width(), bounds.height(),
          static_cast<int>(create_params.initial_show_state));

  return reinterpret_cast<BrowserWindowInterface*>(
      Java_BrowserWindowCreatorBridge_createBrowserWindow(env,
                                                          j_create_params));
}

void CreateBrowserWindow(
    BrowserWindowCreateParams create_params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  // TODO(http://crbug.com/424860292): Implement this on Android.
  NOTIMPLEMENTED();
}

BrowserWindowInterface::CreationStatus GetBrowserWindowCreationStatusForProfile(
    Profile& profile) {
  if (profile.ShutdownStarted()) {
    return BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable;
  }

  return BrowserWindowInterface::CreationStatus::kOk;
}
