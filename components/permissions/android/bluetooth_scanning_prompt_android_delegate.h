// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "components/security_state/core/security_state.h"

namespace content {
class WebContents;
}

namespace permissions {

// Provides embedder-level information to BluetoothScanningPromptAndroid.
class BluetoothScanningPromptAndroidDelegate {
 public:
  virtual ~BluetoothScanningPromptAndroidDelegate() = default;

  // Returns the associated BluetoothScanningPromptAndroidDelegate Java object.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;

  // See security_state::GetSecurityLevel.
  virtual security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) = 0;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_
