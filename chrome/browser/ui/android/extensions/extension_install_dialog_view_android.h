// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

namespace content {
class WebContents;
}

namespace extensions {

// Android implementation of the extension install dialog. Owned by its Java
// peer (ExtensionInstallDialogBridge.java), which deletes it when the dialog
// is dismissed.
class ExtensionInstallDialogViewAndroid {
 public:
  static void Show(content::WebContents* web_contents,
                   std::unique_ptr<InstallPromptData> prompt,
                   ExtensionInstallPrompt::DoneCallback done_callback,
                   ui::WindowAndroid* window_android);

  ExtensionInstallDialogViewAndroid(
      content::WebContents* web_contents,
      std::unique_ptr<InstallPromptData> prompt,
      ExtensionInstallPrompt::DoneCallback done_callback);
  ExtensionInstallDialogViewAndroid(const ExtensionInstallDialogViewAndroid&) =
      delete;
  const ExtensionInstallDialogViewAndroid& operator=(
      const ExtensionInstallDialogViewAndroid&) = delete;
  ~ExtensionInstallDialogViewAndroid();

  // JNI methods.
  void OnDialogAccepted(const std::string& justification_text,
                        bool with_withheld_permissions);
  void OnDialogCanceled();
  void OnDialogDismissed();
  void OnStoreLinkClicked(const std::string& url);

 private:
  // Builds java PropertyModel from `prompt_`.
  void BuildPropertyModel();

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<InstallPromptData> prompt_;
  ExtensionInstallPrompt::DoneCallback done_callback_;

  jni_zero::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_INSTALL_DIALOG_VIEW_ANDROID_H_
