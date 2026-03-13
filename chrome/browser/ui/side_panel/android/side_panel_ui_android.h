// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_SIDE_PANEL_UI_ANDROID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_SIDE_PANEL_UI_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

// Android implementation of `SidePanelUIBase`.
//
// TODO(crbug.com/491597112): Inherit from `SidePanelUIBase` once it is compiled
// into Android.
class SidePanelUIAndroid {
 public:
  SidePanelUIAndroid(JNIEnv* env,
                     const base::android::JavaRef<jobject>& java_bridge);

  // TODO(crbug.com/491597112): This should override `SidePanelUIBase` virtual
  // destructor once this class inherits from it.
  ~SidePanelUIAndroid();

  SidePanelUIAndroid(const SidePanelUIAndroid&) = delete;
  SidePanelUIAndroid& operator=(const SidePanelUIAndroid&) = delete;

  // Implements Java `SidePanelUIAndroidBridge.Natives#destroy`.
  void Destroy(JNIEnv* env);

 private:
  base::android::ScopedJavaLocalRef<jobject> java_bridge() const;

  // A weak reference to the Java `SidePanelUIAndroidBridge`, which is the sole
  // owner of `SidePanelUIAndroid`.
  JavaObjectWeakGlobalRef java_bridge_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_SIDE_PANEL_UI_ANDROID_H_
