// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "chrome/browser/extensions/extension_install_prompt.h"

namespace extensions {

// Android implementation of the extension install dialog.
class ExtensionInstallDialogViewAndroid {
 public:
  ExtensionInstallDialogViewAndroid(
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt,
      ExtensionInstallPrompt::DoneCallback done_callback);
  ExtensionInstallDialogViewAndroid(const ExtensionInstallDialogViewAndroid&) =
      delete;
  const ExtensionInstallDialogViewAndroid& operator=(
      const ExtensionInstallDialogViewAndroid&) = delete;
  ~ExtensionInstallDialogViewAndroid();

  void ShowDialog(ui::WindowAndroid* window_android);

  // JNI methods.
  void OnDialogAccepted(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& justification_text);
  void OnDialogCanceled(JNIEnv* env);
  void OnDialogDismissed(JNIEnv* env);
  void Destroy(JNIEnv* env);

 private:
  // Builds java PropertyModel from `prompt_`.
  void BuildPropertyModel();

  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt_;
  ExtensionInstallPrompt::DoneCallback done_callback_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_ANDROID_H_
