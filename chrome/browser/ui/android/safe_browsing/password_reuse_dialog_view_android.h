// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SAFE_BROWSING_PASSWORD_REUSE_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_SAFE_BROWSING_PASSWORD_REUSE_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class WindowAndroid;
}

namespace safe_browsing {

class PasswordReuseControllerAndroid;

// Modal dialog to display password reuse warning. Directly connected with
// SafeBrowsingPasswordReuseDialogBridge on Java side.
// Owned by |PasswordReuseControllerAndroid|.
class PasswordReuseDialogViewAndroid {
 public:
  explicit PasswordReuseDialogViewAndroid(
      PasswordReuseControllerAndroid* controller);

  PasswordReuseDialogViewAndroid(const PasswordReuseDialogViewAndroid&) =
      delete;
  PasswordReuseDialogViewAndroid& operator=(
      const PasswordReuseDialogViewAndroid&) = delete;

  // Destructor must delete its Java counterpart.
  ~PasswordReuseDialogViewAndroid();

  // Called from native to Java.
  void Show(ui::WindowAndroid* window_android);

  // Called from Java to native.
  void CheckPasswords(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void Ignore(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Close(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // The controller which owns this dialog and handles the dialog events.
  // |controller_| owns |this|.
  raw_ptr<PasswordReuseControllerAndroid> controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_ANDROID_SAFE_BROWSING_PASSWORD_REUSE_DIALOG_VIEW_ANDROID_H_
