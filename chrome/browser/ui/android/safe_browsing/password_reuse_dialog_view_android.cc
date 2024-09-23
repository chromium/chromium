// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/safe_browsing/password_reuse_dialog_view_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SafeBrowsingPasswordReuseDialogBridge_jni.h"

namespace safe_browsing {

PasswordReuseDialogViewAndroid::PasswordReuseDialogViewAndroid(
    PasswordReuseControllerAndroid* controller)
    : controller_(controller) {}

PasswordReuseDialogViewAndroid::~PasswordReuseDialogViewAndroid() {
  Java_SafeBrowsingPasswordReuseDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void PasswordReuseDialogViewAndroid::Show(ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_SafeBrowsingPasswordReuseDialogBridge_create(
      env, window_android->GetJavaObject(), reinterpret_cast<intptr_t>(this)));

  std::u16string warning_detail_text = controller_->GetWarningDetailText();

  Java_SafeBrowsingPasswordReuseDialogBridge_showDialog(
      env, java_object_, controller_->GetTitle(), warning_detail_text,
      controller_->GetPrimaryButtonText(),
      controller_->GetSecondaryButtonText());
}

void PasswordReuseDialogViewAndroid::CheckPasswords(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->ShowCheckPasswords();
}

void PasswordReuseDialogViewAndroid::Ignore(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->IgnoreDialog();
}

void PasswordReuseDialogViewAndroid::Close(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->CloseDialog();
}

}  // namespace safe_browsing
