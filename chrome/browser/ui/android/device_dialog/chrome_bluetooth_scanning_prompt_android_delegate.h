// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_CHROME_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_CHROME_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_

#include "components/permissions/android/bluetooth_scanning_prompt_android_delegate.h"

#include "base/android/scoped_java_ref.h"

class Profile;

// The implementation of BluetoothScanningPromptAndroidDelegate for Chrome.
class ChromeBluetoothScanningPromptAndroidDelegate
    : public permissions::BluetoothScanningPromptAndroidDelegate {
 public:
  explicit ChromeBluetoothScanningPromptAndroidDelegate(Profile* profile);

  ChromeBluetoothScanningPromptAndroidDelegate(
      const ChromeBluetoothScanningPromptAndroidDelegate&) = delete;
  ChromeBluetoothScanningPromptAndroidDelegate& operator=(
      const ChromeBluetoothScanningPromptAndroidDelegate&) = delete;

  ~ChromeBluetoothScanningPromptAndroidDelegate() override;

  // permissions::BluetoothScanningPromptAndroidDelegate implementation:
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_CHROME_BLUETOOTH_SCANNING_PROMPT_ANDROID_DELEGATE_H_
