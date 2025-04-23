// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_SERIAL_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_SERIAL_CHOOSER_DIALOG_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/permissions/chooser_controller.h"
#include "components/security_state/core/security_state.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class RenderFrameHost;
}

// Represents a way to ask the user to select a serial port from a list of
// options.
class SerialChooserDialogAndroid : public permissions::ChooserController::View {
 public:
  class JavaDialog {
   public:
    explicit JavaDialog(const base::android::JavaRef<jobject>& dialog);
    virtual ~JavaDialog();

    JavaDialog(const JavaDialog&) = delete;
    JavaDialog& operator=(const JavaDialog&) = delete;

    virtual void Close();
    virtual void SetIdleState() const;
    virtual void AddDevice(const std::string& item_id,
                           const std::u16string& device_name) const;
    virtual void RemoveDevice(const std::string& item_id) const;
    virtual void OnAdapterEnabledChanged(bool enabled) const;
    virtual void OnAdapterAuthorizationChanged(bool authorized) const;

   private:
    base::android::ScopedJavaGlobalRef<jobject> j_dialog_;
  };

  // The callback type for creating the java dialog object.
  using CreateJavaDialogCallback =
      base::OnceCallback<std::unique_ptr<JavaDialog>(
          JNIEnv*,
          const base::android::JavaRef<jobject>&,  // Java Type: WindowAndroid
          const std::u16string&,
          security_state::SecurityLevel,
          const base::android::JavaRef<jobject>&,  // Java Type: Profile
          SerialChooserDialogAndroid*)>;

  // Creates and shows the dialog. Will return nullptr if the dialog was not
  // displayed. Otherwise |on_close| will be called when the dialog is
  // dismissed.
  static std::unique_ptr<SerialChooserDialogAndroid> Create(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<permissions::ChooserController> controller,
      base::OnceClosure on_close);

  static std::unique_ptr<SerialChooserDialogAndroid> CreateForTesting(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<permissions::ChooserController> controller,
      base::OnceClosure on_close,
      SerialChooserDialogAndroid::CreateJavaDialogCallback
          create_java_dialog_callback);

  explicit SerialChooserDialogAndroid(
      std::unique_ptr<permissions::ChooserController> controller,
      base::OnceClosure on_close);

  SerialChooserDialogAndroid(const SerialChooserDialogAndroid&) = delete;
  SerialChooserDialogAndroid& operator=(const SerialChooserDialogAndroid&) =
      delete;

  ~SerialChooserDialogAndroid() override;

  // Start listing devices. Used when Java views finish obtaining permissions.
  void ListDevices(JNIEnv* env);

  // permissions::ChooserController::View implementation
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnRefreshStateChanged(bool refreshing) override;
  void OnAdapterAuthorizationChanged(bool authorized) override;

  // Report the dialog's result.
  void OnItemSelected(JNIEnv* env, std::string& item_id);
  void OnDialogCancelled(JNIEnv* env);
  void OpenSerialHelpPage(JNIEnv* env);
  void OpenAdapterOffHelpPage(JNIEnv* env);
  void OpenBluetoothPermissionHelpPage(JNIEnv* env);

 private:
  // Called when the chooser dialog is closed.
  void Cancel();

  static std::unique_ptr<SerialChooserDialogAndroid> CreateInternal(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<permissions::ChooserController> controller,
      base::OnceClosure on_close,
      SerialChooserDialogAndroid::CreateJavaDialogCallback
          create_java_dialog_callback);

  std::unique_ptr<permissions::ChooserController> controller_;
  base::OnceClosure on_close_;

  // The Java dialog code expects items to have unique string IDs while the
  // ChooserController code refers to devices by their position in the list.
  int next_item_id_ = 0;
  std::vector<std::string> item_id_map_;

  std::unique_ptr<JavaDialog> java_dialog_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_SERIAL_CHOOSER_DIALOG_ANDROID_H_
