// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"
#include "content/public/browser/web_contents.h"

// Represents a Bluetooth scanning prompt to ask the user permission to
// allow a site to receive Bluetooth advertisement packets from Bluetooth
// devices. This implementation is for Android.
class BluetoothScanningPromptAndroid : public content::BluetoothScanningPrompt {
 public:
  BluetoothScanningPromptAndroid(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler);
  ~BluetoothScanningPromptAndroid() override;

  // content::BluetoothScanningPrompt:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name) override;

  // Report the dialog's result.
  void OnDialogFinished(JNIEnv* env,
                        jint event_type);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  content::WebContents* web_contents_;
  content::BluetoothScanningPrompt::EventHandler event_handler_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothScanningPromptAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_
