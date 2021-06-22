// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_BLUETOOTH_CHOOSER_ANDROID_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_BLUETOOTH_CHOOSER_ANDROID_DELEGATE_H_

#include "base/macros.h"

#include "base/android/scoped_java_ref.h"
#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}

// Provides embedder-level information to BluetoothChooserAndroid.
class BluetoothChooserAndroidDelegate {
 public:
  virtual ~BluetoothChooserAndroidDelegate() = default;

  // Returns the associated BluetoothChooserAndroidDelegate Java object.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;

  // See security_state::GetSecurityLevel.
  virtual security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) = 0;
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_BLUETOOTH_CHOOSER_ANDROID_DELEGATE_H_
