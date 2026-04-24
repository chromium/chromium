// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

// These must be last.
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBrowserWindowCreateParamsImpl_jni.h"
#include "chrome/browser/ui/browser_window/internal/jni/BrowserWindowCreatorBridge_jni.h"

BrowserWindowInterface* CreateBrowserWindow(
    BrowserWindowCreateParams create_params) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const gfx::Rect& bounds = create_params.initial_bounds;

  // This code is still responsible for the WebContents until it receives a
  // signal that the window creation is possible.
  base::android::ScopedJavaLocalRef<jobject> j_create_params =
      Java_AndroidBrowserWindowCreateParamsImpl_create(
          env, static_cast<int>(create_params.type),
          create_params.profile->GetJavaObject(), bounds.x(), bounds.y(),
          bounds.right(), bounds.bottom(),
          static_cast<int>(create_params.initial_show_state),
          create_params.web_contents.get());

  int64_t window_ptr =
      Java_BrowserWindowCreatorBridge_createBrowserWindow(env, j_create_params);

  if (window_ptr != 0) {
    // Java has created a detached Tab which has assumed ownership of this
    // WebContents (and is being reparented asynchronously).
    create_params.web_contents.release();
  }

  return reinterpret_cast<BrowserWindowInterface*>(window_ptr);
}

void CreateBrowserWindow(
    BrowserWindowCreateParams create_params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const gfx::Rect& bounds = create_params.initial_bounds;

  // This code is still responsible for the WebContents until it receives a
  // signal that the window creation is possible.
  base::android::ScopedJavaLocalRef<jobject> j_create_params =
      Java_AndroidBrowserWindowCreateParamsImpl_create(
          env, static_cast<int>(create_params.type),
          create_params.profile->GetJavaObject(), bounds.x(), bounds.y(),
          bounds.right(), bounds.bottom(),
          static_cast<int>(create_params.initial_show_state),
          create_params.web_contents.get());

  // The callback will be invoked with the native pointer of the created browser
  // window. The pointer is represented as a int64_t in Java.
  base::OnceCallback<void(int64_t)> jlong_callback = base::BindOnce(
      [](base::OnceCallback<void(BrowserWindowInterface*)> cb,
         std::unique_ptr<content::WebContents> web_contents, int64_t ptr) {
        if (ptr != 0) {
          // Java has created a detached Tab which has assumed ownership of this
          // WebContents (and is being reparented asynchronously).
          web_contents.release();
        }
        std::move(cb).Run(reinterpret_cast<BrowserWindowInterface*>(ptr));
      },
      std::move(callback), std::move(create_params.web_contents));

  Java_BrowserWindowCreatorBridge_createBrowserWindowAsync(
      env, j_create_params,
      base::android::ToJniCallback(env, std::move(jlong_callback)));
}

BrowserWindowInterface::CreationStatus GetBrowserWindowCreationStatusForProfile(
    Profile& profile) {
  if (profile.ShutdownStarted()) {
    return BrowserWindowInterface::CreationStatus::kErrorShuttingDown;
  }

  if (!IncognitoModePrefs::CanOpenBrowser(&profile) ||
      !profile.AllowsBrowserWindows() ||
      IsProfileDirectoryMarkedForDeletion(profile.GetPath())) {
    return BrowserWindowInterface::CreationStatus::kErrorProfileUnsuitable;
  }

  return BrowserWindowInterface::CreationStatus::kOk;
}

DEFINE_JNI(AndroidBrowserWindowCreateParamsImpl)
DEFINE_JNI(BrowserWindowCreatorBridge)
