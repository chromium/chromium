// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_PASSWORD_GENERATION_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_PASSWORD_GENERATION_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/strings/string16.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"

class PasswordGenerationController;

// Modal dialog displaying a generated password with options to accept or
// reject it. Communicates events to its Java counterpart and passes responses
// back to the |PasswordGenerationController|.
class PasswordGenerationDialogViewAndroid
    : public PasswordGenerationDialogViewInterface {
 public:
  // Builds the UI for the |controller|
  explicit PasswordGenerationDialogViewAndroid(
      PasswordGenerationController* controller);

  ~PasswordGenerationDialogViewAndroid() override;

  // Called to show the dialog. |password| is the generated password.
  void Show(
      base::string16& password,
      base::WeakPtr<password_manager::PasswordManagerDriver>
          target_frame_driver,
      autofill::password_generation::PasswordGenerationType type) override;

  // Called from Java via JNI.
  void PasswordAccepted(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jstring>& password);

  // Called from Java via JNI.
  void PasswordRejected(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);

 private:
  // The controller provides data for this view and owns it.
  PasswordGenerationController* controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // The driver corresponding to the frame for which the generation request was
  // made. Used to ensure that the accepted password message is sent back to the
  // same frame.
  base::WeakPtr<password_manager::PasswordManagerDriver> target_frame_driver_;

  // Whether the dialog was shown for manual generation or not. Used for
  // metrics.
  autofill::password_generation::PasswordGenerationType generation_type_;
  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationDialogViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_PASSWORD_GENERATION_DIALOG_VIEW_ANDROID_H_
