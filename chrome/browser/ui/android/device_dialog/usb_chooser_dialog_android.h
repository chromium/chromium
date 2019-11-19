// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_USB_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_USB_CHOOSER_DIALOG_ANDROID_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"

namespace content {
class RenderFrameHost;
}

// Represents a way to ask the user to select a USB device from a list of
// options.
class UsbChooserDialogAndroid : public ChooserController::View {
 public:
  // Creates and shows the dialog. Will return nullptr if the dialog was not
  // displayed. Otherwise |on_close| will be called when the user closes the
  // dialog.
  static std::unique_ptr<UsbChooserDialogAndroid> Create(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<ChooserController> controller,
      base::OnceClosure on_close);

  explicit UsbChooserDialogAndroid(
      std::unique_ptr<ChooserController> controller,
      base::OnceClosure on_close);
  ~UsbChooserDialogAndroid() override;

  // ChooserController::View implementation
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnRefreshStateChanged(bool refreshing) override;

  // Report the dialog's result.
  void OnItemSelected(JNIEnv* env,
                      const base::android::JavaParamRef<jstring>& item_id);
  void OnDialogCancelled(JNIEnv* env);
  void LoadUsbHelpPage(JNIEnv* env);

 private:
  // Called when the chooser dialog is closed.
  void Cancel();

  std::unique_ptr<ChooserController> controller_;
  base::OnceClosure on_close_;

  // The Java dialog code expects items to have unique string IDs while the
  // ChooserController code refers to devices by their position in the list.
  int next_item_id_ = 0;
  std::vector<std::string> item_id_map_;

  base::android::ScopedJavaGlobalRef<jobject> java_dialog_;

  DISALLOW_COPY_AND_ASSIGN(UsbChooserDialogAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_DEVICE_DIALOG_USB_CHOOSER_DIALOG_ANDROID_H_
