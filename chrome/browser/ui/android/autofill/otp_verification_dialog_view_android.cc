// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/otp_verification_dialog_view_android.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/autofill/internal/jni_headers/OtpVerificationDialogBridge_jni.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_otp_input_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;

namespace autofill {

OtpVerificationDialogViewAndroid::OtpVerificationDialogViewAndroid(
    CardUnmaskOtpInputDialogController* controller)
    : controller_(controller) {
  DCHECK(controller);
}

OtpVerificationDialogViewAndroid::~OtpVerificationDialogViewAndroid() {
  // Inform |controller_| of the dialog's destruction.
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

// static
CardUnmaskOtpInputDialogView* CardUnmaskOtpInputDialogView::CreateAndShow(
    CardUnmaskOtpInputDialogController* controller,
    content::WebContents* web_contents) {
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  if (!view_android) {
    return nullptr;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return nullptr;
  }
  OtpVerificationDialogViewAndroid* dialog_view =
      new OtpVerificationDialogViewAndroid(controller);
  // Return the dialog only if we were able to show it.
  if (dialog_view->ShowDialog(window_android)) {
    return dialog_view;
  } else {
    return nullptr;
  }
}

void OtpVerificationDialogViewAndroid::ShowPendingState() {
  // For Android, the Java code takes care of showing the pending state after
  // user submits the OTP.
  NOTREACHED();
}

void OtpVerificationDialogViewAndroid::ShowErrorMessage(
    const std::u16string error_message) {
  DCHECK(java_object_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_OtpVerificationDialogBridge_showOtpErrorMessage(
      env, java_object_, ConvertUTF16ToJavaString(env, error_message));
}

void OtpVerificationDialogViewAndroid::OnControllerDestroying() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_OtpVerificationDialogBridge_dismissDialog(env, java_object_);
  } else {
    delete this;
  }
}

void OtpVerificationDialogViewAndroid::OnDialogDismissed(JNIEnv* env) {
  // Since the destructor would call controller->OnDialogClosed, calling delete
  // on this is sufficient.
  delete this;
}

void OtpVerificationDialogViewAndroid::OnConfirm(
    JNIEnv* env,
    const JavaParamRef<jstring>& otp) {
  // TODO(crbug.com/1196021): Implement method in
  // CardUnmaskOtpInputDialogController
}
void OtpVerificationDialogViewAndroid::OnNewOtpRequested(JNIEnv* env) {
  // TODO(crbug.com/1196021): Implement method in
  // CardUnmaskOtpInputDialogController
}

bool OtpVerificationDialogViewAndroid::ShowDialog(
    ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_OtpVerificationDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
  if (java_object_.is_null()) {
    return false;
  }
  Java_OtpVerificationDialogBridge_showDialog(
      env, java_object_, controller_->GetExpectedOtpLength());
  return true;
}

}  // namespace autofill
