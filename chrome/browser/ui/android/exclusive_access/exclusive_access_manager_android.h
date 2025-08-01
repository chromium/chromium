// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_MANAGER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_MANAGER_ANDROID_H_

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "third_party/jni_zero/jni_zero.h"

// Exclusive Access Manager Android class is the Exclusive Access Manager
// wrapper used for synchronization of Pointer Lock, Keyboard Lock and
// Fullscreen features. The main responsibilities of EAM are to monitor which
// features are currently in use and when the features exit criteria are met
// (e.g. ESC key is pressed). This is done by a set of custom controllers for
// each feature.
// ExclusiveAccessManagerAndroid uses the ExclusiveAccessContextAndroid which
// as the delegate.
class ExclusiveAccessManagerAndroid {
 public:
  ExclusiveAccessManagerAndroid(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& jeam,
      const jni_zero::JavaRef<jobject>& j_fullscreen_manager,
      const jni_zero::JavaRef<jobject>& j_activity_tab_provider);
  ~ExclusiveAccessManagerAndroid();

  void Destroy(JNIEnv* env);

  void EnterFullscreenModeForTab(JNIEnv* env,
                                 jlong requesting_frame,
                                 bool prefersNavigationBar,
                                 bool prefersStatusBar);

  void ExitFullscreenModeForTab(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& jweb_contents);

  bool IsFullscreenForTabOrPending(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& jweb_contents);

  bool PreHandleKeyboardEvent(JNIEnv* env, jlong nativeKeyEvent);

  void RequestKeyboardLock(JNIEnv* env,
                           const jni_zero::JavaRef<jobject>& jweb_contents,
                           bool escKeyLocked);

  void CancelKeyboardLockRequest(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& jweb_contents);

 private:
  // Our global reference to the Java ExclusiveAccessManagerAndroid.
  base::android::ScopedJavaGlobalRef<jobject> j_eam_;
  std::unique_ptr<ExclusiveAccessContext> eac_;
  ExclusiveAccessManager eam_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_MANAGER_ANDROID_H_
