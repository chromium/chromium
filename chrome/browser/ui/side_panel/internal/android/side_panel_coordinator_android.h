// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

// Android implementation of `SidePanelUIBase`.
//
// It's named as `SidePanelCoordinatorAndroid` to be consistent with
// `SidePanelCoordinator`, which is the main `SidePanelUIBase` implementation on
// Windows, Mac, and Linux.
//
// The word "coordinator" does not refer to the "coordinator" component in
// Chrome Android MVC UI Architecture:
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_overview.md
//
// TODO(crbug.com/491597112): Inherit from `SidePanelUIBase` once it is compiled
// into Android.
class SidePanelCoordinatorAndroid {
 public:
  SidePanelCoordinatorAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_coordinator);

  // TODO(crbug.com/491597112): This should override `SidePanelUIBase` virtual
  // destructor once this class inherits from it.
  ~SidePanelCoordinatorAndroid();

  SidePanelCoordinatorAndroid(const SidePanelCoordinatorAndroid&) = delete;
  SidePanelCoordinatorAndroid& operator=(const SidePanelCoordinatorAndroid&) =
      delete;

  // Implements Java `SidePanelCoordinatorAndroid.Natives#destroy`.
  void Destroy(JNIEnv* env);

 private:
  base::android::ScopedJavaLocalRef<jobject> java_coordinator() const;

  // A weak reference to the Java `SidePanelCoordinatorAndroid`, which is
  // the sole owner of the C++ `SidePanelCoordinatorAndroid`.
  JavaObjectWeakGlobalRef java_coordinator_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
