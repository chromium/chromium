// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_HID_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_HID_CHOOSER_DIALOG_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chrome/android/chrome_jni_headers/HidChooserDialog_shared_jni.h"
#include "components/permissions/chooser_controller.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class RenderFrameHost;
}

// Represents a way to ask the user to select a HID device from a list of
// options.
class HidChooserDialogAndroid : public permissions::ChooserController::View {
 public:
  // The callback type for creating the java dialog object.
  using CreateJavaDialogCallback =
      base::OnceCallback<base::android::ScopedJavaLocalRef<jobject>(
          JNIEnv*,
          const base::android::JavaRef<jobject>&,
          const std::u16string&,
          JniIntWrapper,
          const base::android::JavaRef<jobject>&,
          int64_t)>;

  // Creates and shows the dialog. Returns a OnceClosure that can be called to
  // cancel and close the dialog, or an empty closure if the dialog could not
  // be created.
  static base::OnceClosure Create(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<permissions::ChooserController> controller);

  static base::OnceClosure CreateForTesting(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<permissions::ChooserController> controller,
      CreateJavaDialogCallback create_java_dialog_callback);

  explicit HidChooserDialogAndroid(
      std::unique_ptr<permissions::ChooserController> controller);

  HidChooserDialogAndroid(const HidChooserDialogAndroid&) = delete;
  HidChooserDialogAndroid& operator=(const HidChooserDialogAndroid&) = delete;

  ~HidChooserDialogAndroid() override;

  // permissions::ChooserController::View implementation
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnRefreshStateChanged(bool refreshing) override;

  // Report the dialog's result.
  void OnItemSelected(JNIEnv* env, const std::string& item_id);
  void OnDialogCancelled(JNIEnv* env);
  void LoadHidHelpPage(JNIEnv* env);

 private:
  // Called when the chooser dialog is closed.
  void Cancel();

  static base::OnceClosure CreateInternal(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<permissions::ChooserController> controller,
      CreateJavaDialogCallback create_java_dialog_callback);

  std::unique_ptr<permissions::ChooserController> controller_;

  // The Java dialog code expects items to have unique string IDs while the
  // ChooserController code refers to devices by their position in the list.
  int next_item_id_ = 0;
  std::vector<std::string> item_id_map_;

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  bool item_selected_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_HID_CHOOSER_DIALOG_ANDROID_H_
