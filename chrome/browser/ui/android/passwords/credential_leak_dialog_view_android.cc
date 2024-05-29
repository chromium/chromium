// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/CredentialLeakDialogBridge_jni.h"

CredentialLeakDialogViewAndroid::CredentialLeakDialogViewAndroid(
    CredentialLeakControllerAndroid* controller)
    : controller_(controller) {}

CredentialLeakDialogViewAndroid::~CredentialLeakDialogViewAndroid() {
  Java_CredentialLeakDialogBridge_destroy(base::android::AttachCurrentThread(),
                                          java_object_);
}

void CredentialLeakDialogViewAndroid::Show(ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_CredentialLeakDialogBridge_create(
      env, window_android->GetJavaObject(), reinterpret_cast<intptr_t>(this)));

  Java_CredentialLeakDialogBridge_showDialog(
      env, java_object_, controller_->GetTitle(), controller_->GetDescription(),
      controller_->GetAcceptButtonLabel(),
      controller_->ShouldShowCancelButton()
          ? base::android::ConvertUTF16ToJavaString(
                env, controller_->GetCancelButtonLabel())
          : nullptr);
}

void CredentialLeakDialogViewAndroid::Accepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnAcceptDialog();
}

void CredentialLeakDialogViewAndroid::Cancelled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnCancelDialog();
}

void CredentialLeakDialogViewAndroid::Closed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  controller_->OnCloseDialog();
}
