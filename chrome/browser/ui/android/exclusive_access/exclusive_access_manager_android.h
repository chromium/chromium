// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_MANAGER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_MANAGER_ANDROID_H_

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "third_party/jni_zero/jni_zero.h"

class ExclusiveAccessContextAndroid;

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
      const jni_zero::JavaRef<jobject>& j_context,
      const jni_zero::JavaRef<jobject>& j_fullscreen_manager,
      const jni_zero::JavaRef<jobject>& j_activity_tab_provider);
  ~ExclusiveAccessManagerAndroid();

  void Destroy(JNIEnv* env);

  void EnterFullscreenModeForTab(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& jrender_frame_host_android,
      bool prefersNavigationBar,
      bool prefersStatusBar,
      jlong displayId);

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

  bool IsKeyboardLocked(JNIEnv* env);

  void RequestPointerLock(JNIEnv* env,
                          const jni_zero::JavaRef<jobject>& jweb_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target);

  void LostPointerLock(JNIEnv* env);

  bool IsPointerLocked(JNIEnv* env);

  void ExitExclusiveAccess(JNIEnv* env);

  bool HasExclusiveAccess(JNIEnv* env);

  void OnTabDeactivated(JNIEnv* env,
                        const jni_zero::JavaRef<jobject>& jweb_contents);

  void OnTabDetachedFromView(JNIEnv* env,
                             const jni_zero::JavaRef<jobject>& jweb_contents);

  void OnTabClosing(JNIEnv* env,
                    const jni_zero::JavaRef<jobject>& jweb_contents);

  void ForceActiveTab(JNIEnv* env, const jni_zero::JavaRef<jobject>& j_tab);

 private:
  // Our global reference to the Java ExclusiveAccessManagerAndroid.
  base::android::ScopedJavaGlobalRef<jobject> j_eam_;
  std::unique_ptr<ExclusiveAccessContextAndroid> eac_;
  ExclusiveAccessManager eam_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_MANAGER_ANDROID_H_
