// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

namespace permissions {

class BluetoothScanningPromptAndroidDelegate;

// Represents a Bluetooth scanning prompt to ask the user permission to
// allow a site to receive Bluetooth advertisement packets from Bluetooth
// devices. This implementation is for Android.
class BluetoothScanningPromptAndroid : public content::BluetoothScanningPrompt {
 public:
  // The callback type for creating the java dialog object.
  using CreateJavaDialogCallback =
      base::OnceCallback<base::android::ScopedJavaLocalRef<jobject>(
          JNIEnv*,
          const base::android::JavaRef<jobject>&,
          const base::android::JavaRef<jstring>&,
          JniIntWrapper,
          const base::android::JavaRef<jobject>&,
          jlong)>;

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

  static std::unique_ptr<BluetoothScanningPromptAndroid> CreateForTesting(
      content::RenderFrameHost* frame,
      const EventHandler& event_handler,
      std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate,
      CreateJavaDialogCallback create_java_dialog_callback) {
    // Using `new` to access a non-public constructor.
    return base::WrapUnique(new BluetoothScanningPromptAndroid(
        frame, event_handler, std::move(delegate),
        std::move(create_java_dialog_callback)));
  }

 private:
  BluetoothScanningPromptAndroid(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler,
      std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate,
      CreateJavaDialogCallback create_java_dialog_callback);

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  raw_ptr<content::WebContents> web_contents_;
  content::BluetoothScanningPrompt::EventHandler event_handler_;
  std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_SCANNING_PROMPT_ANDROID_H_
