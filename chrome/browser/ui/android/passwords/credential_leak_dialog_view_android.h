// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class WindowAndroid;
}

class CredentialLeakControllerAndroid;

// Modal dialog displaying a warning for the user when an entered credential was
// detected to have been part of a leak. Communicated with its Java counterpart
// and passes responses back to the |CredentialLeakControllerAndroid|.
class CredentialLeakDialogViewAndroid {
 public:
  explicit CredentialLeakDialogViewAndroid(
      CredentialLeakControllerAndroid* controller);

  CredentialLeakDialogViewAndroid(const CredentialLeakDialogViewAndroid&) =
      delete;
  CredentialLeakDialogViewAndroid& operator=(
      const CredentialLeakDialogViewAndroid&) = delete;

  ~CredentialLeakDialogViewAndroid();

  // Called to create and show the dialog.
  void Show(ui::WindowAndroid* window_android);

  // Called from Java via JNI.
  void Accepted(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called from Java via JNI.
  void Cancelled(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called from Java via JNI.
  void Closed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // The controller which owns this dialog and handles the dialog events.
  raw_ptr<CredentialLeakControllerAndroid> controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_CREDENTIAL_LEAK_DIALOG_VIEW_ANDROID_H_
