// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_DEVICE_LOCK_BRIDGE_H_
#define COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_DEVICE_LOCK_BRIDGE_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

namespace ui {
class WindowAndroid;
}

// The glue for the Java-side implementation of DeviceLockBridge.
class DeviceLockBridge {
 public:
  DeviceLockBridge();
  virtual ~DeviceLockBridge();

  using DeviceLockRequirementMetCallback = base::OnceCallback<void(bool)>;

  // Launches the Device Lock setup UI (explainer dialog and PIN/password setup
  // flow) if needed before allowing users to continue.
  virtual void LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      ui::WindowAndroid* window_android,
      DeviceLockRequirementMetCallback callback);

  // Returns true iff the device requires a device lock (ex: pin/password) and
  // does not have one set or requires a device lock but hasn't seen the
  // explainer dialog before.
  virtual bool ShouldShowDeviceLockUi();

  // Returns true iff a device lock (ex: pin/password) is needed for sign in,
  // sync, and autofill flows.
  virtual bool RequiresDeviceLock();

 private:
  // Returns true iff the device has a device lock (ex: pin/password).
  bool IsDeviceSecure();

  // Returns true iff the device lock page has already been passed (i.e. the
  // device lock page has been shown to and affirmatively acknowledged by the
  // user).
  bool DeviceLockPageHasBeenPassed();
};

#endif  // COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_DEVICE_LOCK_BRIDGE_H_
