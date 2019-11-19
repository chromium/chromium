// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_DIALOG_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog.h"
#include "content/public/browser/javascript_dialog_manager.h"

using base::android::ScopedJavaGlobalRef;

// An Android version of a JavaScript dialog that automatically dismisses itself
// when the user switches away to a different tab, used for WebContentses that
// are browser tabs.
class JavaScriptDialogAndroid : public JavaScriptDialog {
 public:
  ~JavaScriptDialogAndroid() override;

  // Note on the two callbacks: |dialog_callback_on_button_clicked| is for the
  // case where user responds to the dialog. |dialog_callback_on_cancelled| is
  // for the case where user cancels the dialog without interacting with the
  // dialog (e.g. clicks the navigate back button on Android).
  static base::WeakPtr<JavaScriptDialogAndroid> Create(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback
          callback_on_button_clicked,
      base::OnceClosure callback_on_cancelled);

  // JavaScriptDialog:
  void CloseDialogWithoutCallback() override;
  base::string16 GetUserInput() override;

  void Accept(JNIEnv* env,
              const base::android::JavaParamRef<jobject>&,
              const base::android::JavaParamRef<jstring>& prompt);
  void Cancel(JNIEnv* env,
              const base::android::JavaParamRef<jobject>&,
              jboolean button_clicked);

 private:
  friend class JavaScriptDialog;

  JavaScriptDialogAndroid(content::WebContents* parent_web_contents,
                          content::WebContents* alerting_web_contents,
                          const base::string16& title,
                          content::JavaScriptDialogType dialog_type,
                          const base::string16& message_text,
                          const base::string16& default_prompt_text,
                          content::JavaScriptDialogManager::DialogClosedCallback
                              callback_on_button_clicked,
                          base::OnceClosure callback_on_cancelled);

  std::unique_ptr<JavaScriptDialogAndroid> dialog_;
  ScopedJavaGlobalRef<jobject> dialog_jobject_;
  JavaObjectWeakGlobalRef jwindow_weak_ref_;

  content::JavaScriptDialogManager::DialogClosedCallback
      callback_on_button_clicked_;
  base::OnceClosure callback_on_cancelled_;

  base::WeakPtrFactory<JavaScriptDialogAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_DIALOG_ANDROID_H_
