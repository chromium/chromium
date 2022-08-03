// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_qr_code_native_delegate.h"

#include "components/autofill_assistant/android/jni_headers/AssistantQrCodeNativeDelegate_jni.h"
#include "components/autofill_assistant/browser/android/ui_controller_android.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"
#include "components/autofill_assistant/browser/value_util.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantQrCodeNativeDelegate::AssistantQrCodeNativeDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_qr_code_native_delegate_ =
      Java_AssistantQrCodeNativeDelegate_Constructor(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantQrCodeNativeDelegate::~AssistantQrCodeNativeDelegate() {
  Java_AssistantQrCodeNativeDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_qr_code_native_delegate_);
}

void AssistantQrCodeNativeDelegate::OnScanResult(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jvalue) {
  ui_controller_->OnQrCodeScanFinished(
      ClientStatus(ACTION_APPLIED),
      SimpleValue(ui_controller_android_utils::SafeConvertJavaStringToNative(
                      env, jvalue),
                  /*  is_client_side_only= */ true));
}

void AssistantQrCodeNativeDelegate::OnScanCancelled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnQrCodeScanFinished(ClientStatus(QR_CODE_SCAN_CANCELLED),
                                       absl::nullopt);
}

void AssistantQrCodeNativeDelegate::OnScanFailure(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnQrCodeScanFinished(ClientStatus(QR_CODE_SCAN_FAILURE),
                                       absl::nullopt);
}

void AssistantQrCodeNativeDelegate::OnCameraError(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnQrCodeScanFinished(ClientStatus(QR_CODE_SCAN_CAMERA_ERROR),
                                       absl::nullopt);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantQrCodeNativeDelegate::GetJavaObject() const {
  return java_assistant_qr_code_native_delegate_;
}

}  // namespace autofill_assistant
