// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_qr_code_camera_scan_model_wrapper.h"

#include "base/android/jni_string.h"
#include "components/autofill_assistant/android/jni_headers/AssistantQrCodeCameraScanModelWrapper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;

namespace autofill_assistant {

AssistantQrCodeCameraScanModelWrapper::AssistantQrCodeCameraScanModelWrapper() {
  jni_env_ = AttachCurrentThread();
  java_assistant_camera_scan_model_wrapper_ =
      Java_AssistantQrCodeCameraScanModelWrapper_Constructor(jni_env_);
}

AssistantQrCodeCameraScanModelWrapper::
    ~AssistantQrCodeCameraScanModelWrapper() = default;

base::android::ScopedJavaGlobalRef<jobject>
AssistantQrCodeCameraScanModelWrapper::GetModel() const {
  return java_assistant_camera_scan_model_wrapper_;
}

void AssistantQrCodeCameraScanModelWrapper::SetDelegate(
    const base::android::ScopedJavaGlobalRef<jobject>&
        java_qr_code_native_delegate) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setDelegate(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      java_qr_code_native_delegate);
}

void AssistantQrCodeCameraScanModelWrapper::SetToolbarTitle(
    const std::string& title_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setToolbarTitle(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, title_text));
}

void AssistantQrCodeCameraScanModelWrapper::SetPermissionText(
    const std::string& permission_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setPermissionText(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, permission_text));
}

void AssistantQrCodeCameraScanModelWrapper::SetPermissionButtonText(
    const std::string& permission_button_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setPermissionButtonText(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, permission_button_text));
}

void AssistantQrCodeCameraScanModelWrapper::SetOpenSettingsText(
    const std::string& open_settings_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setOpenSettingsText(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, open_settings_text));
}

void AssistantQrCodeCameraScanModelWrapper::SetOpenSettingsButtonText(
    const std::string& open_settings_button_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setOpenSettingsButtonText(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, open_settings_button_text));
}

void AssistantQrCodeCameraScanModelWrapper::SetCameraPreviewInstructionText(
    const std::string& camera_preview_instruction_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setOverlayInstructionText(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, camera_preview_instruction_text));
}

void AssistantQrCodeCameraScanModelWrapper::SetCameraPreviewSecurityText(
    const std::string& camera_preview_security_text) const {
  Java_AssistantQrCodeCameraScanModelWrapper_setOverlaySecurityText(
      jni_env_, java_assistant_camera_scan_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, camera_preview_security_text));
}
}  // namespace autofill_assistant