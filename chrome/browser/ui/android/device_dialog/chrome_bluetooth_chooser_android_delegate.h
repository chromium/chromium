// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_CHROME_BLUETOOTH_CHOOSER_ANDROID_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_CHROME_BLUETOOTH_CHOOSER_ANDROID_DELEGATE_H_

#include "components/permissions/android/bluetooth_chooser_android_delegate.h"

#include "base/android/scoped_java_ref.h"

class Profile;

// The implementation of BluetoothChooserAndroidDelegate for Chrome.
class ChromeBluetoothChooserAndroidDelegate
    : public permissions::BluetoothChooserAndroidDelegate {
 public:
  explicit ChromeBluetoothChooserAndroidDelegate(Profile* profile);

  ChromeBluetoothChooserAndroidDelegate(
      const ChromeBluetoothChooserAndroidDelegate&) = delete;
  ChromeBluetoothChooserAndroidDelegate& operator=(
      const ChromeBluetoothChooserAndroidDelegate&) = delete;

  ~ChromeBluetoothChooserAndroidDelegate() override;

  // BluetoothChooserAndroidDelegate implementation:
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_CHROME_BLUETOOTH_CHOOSER_ANDROID_DELEGATE_H_
