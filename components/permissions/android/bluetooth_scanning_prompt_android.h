// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

class BluetoothScanningPromptAndroidDelegate;

// Represents a Bluetooth scanning prompt to ask the user permission to
// allow a site to receive Bluetooth advertisement packets from Bluetooth
// devices. This implementation is for Android.
class BluetoothScanningPromptAndroid : public content::BluetoothScanningPrompt {
 public:
  BluetoothScanningPromptAndroid(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler,
      std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate);

  BluetoothScanningPromptAndroid(const BluetoothScanningPromptAndroid&) =
      delete;
  BluetoothScanningPromptAndroid& operator=(
      const BluetoothScanningPromptAndroid&) = delete;

  ~BluetoothScanningPromptAndroid() override;

  // content::BluetoothScanningPrompt:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name) override;

  // Report the dialog's result.
  void OnDialogFinished(JNIEnv* env, jint event_type);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  raw_ptr<content::WebContents> web_contents_;
  content::BluetoothScanningPrompt::EventHandler event_handler_;
  std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_
