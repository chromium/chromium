// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_CHOOSER_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_CHOOSER_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

namespace permissions {

class BluetoothChooserAndroidDelegate;

// Represents a way to ask the user to select a Bluetooth device from a list of
// options.
class BluetoothChooserAndroid : public content::BluetoothChooser {
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

  // Both frame and event_handler must outlive the BluetoothChooserAndroid.
  BluetoothChooserAndroid(
      content::RenderFrameHost* frame,
      const EventHandler& event_handler,
      std::unique_ptr<BluetoothChooserAndroidDelegate> delegate);
  ~BluetoothChooserAndroid() override;

  // content::BluetoothChooser:
  bool CanAskForScanningPermission() override;
  void SetAdapterPresence(AdapterPresence presence) override;
  void ShowDiscoveryState(DiscoveryState state) override;
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override;

  // Report the dialog's result.
  void OnDialogFinished(JNIEnv* env,
                        jint event_type,
                        const base::android::JavaParamRef<jstring>& device_id);

  // Notify bluetooth stack that the search needs to be re-issued.
  void RestartSearch();
  // Calls RestartSearch(). Unused JNI parameters enable calls from Java.
  void RestartSearch(JNIEnv*);

  void ShowBluetoothOverviewLink(JNIEnv* env);
  void ShowBluetoothAdapterOffLink(JNIEnv* env);
  void ShowNeedLocationPermissionLink(JNIEnv* env);

  static std::unique_ptr<BluetoothChooserAndroid> CreateForTesting(
      content::RenderFrameHost* frame,
      const EventHandler& event_handler,
      std::unique_ptr<BluetoothChooserAndroidDelegate> delegate,
      CreateJavaDialogCallback create_java_dialog_callback) {
    // Using `new` to access a non-public constructor.
    return base::WrapUnique(
        new BluetoothChooserAndroid(frame, event_handler, std::move(delegate),
                                    std::move(create_java_dialog_callback)));
  }

 private:
  BluetoothChooserAndroid(
      content::RenderFrameHost* frame,
      const EventHandler& event_handler,
      std::unique_ptr<BluetoothChooserAndroidDelegate> delegate,
      CreateJavaDialogCallback create_java_dialog_callback);

  void OpenURL(const char* url);

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  raw_ptr<content::WebContents> web_contents_;
  BluetoothChooser::EventHandler event_handler_;
  std::unique_ptr<BluetoothChooserAndroidDelegate> delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_BLUETOOTH_CHOOSER_ANDROID_H_
