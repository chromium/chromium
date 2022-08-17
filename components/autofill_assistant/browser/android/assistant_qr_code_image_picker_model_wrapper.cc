// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_qr_code_image_picker_model_wrapper.h"

#include "base/android/jni_string.h"
#include "components/autofill_assistant/android/jni_headers/AssistantQrCodeImagePickerModelWrapper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;

namespace autofill_assistant {

AssistantQrCodeImagePickerModelWrapper::
    AssistantQrCodeImagePickerModelWrapper() {
  jni_env_ = AttachCurrentThread();
  java_assistant_image_picker_model_wrapper_ =
      Java_AssistantQrCodeImagePickerModelWrapper_Constructor(jni_env_);
}

AssistantQrCodeImagePickerModelWrapper::
    ~AssistantQrCodeImagePickerModelWrapper() = default;

base::android::ScopedJavaGlobalRef<jobject>
AssistantQrCodeImagePickerModelWrapper::GetModel() const {
  return java_assistant_image_picker_model_wrapper_;
}

void AssistantQrCodeImagePickerModelWrapper::SetDelegate(
    const base::android::ScopedJavaGlobalRef<jobject>&
        java_qr_code_native_delegate) const {
  Java_AssistantQrCodeImagePickerModelWrapper_setDelegate(
      jni_env_, java_assistant_image_picker_model_wrapper_,
      java_qr_code_native_delegate);
}

void AssistantQrCodeImagePickerModelWrapper::SetToolbarTitle(
    const std::string& title_text) const {
  Java_AssistantQrCodeImagePickerModelWrapper_setToolbarTitle(
      jni_env_, java_assistant_image_picker_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, title_text));
}

void AssistantQrCodeImagePickerModelWrapper::SetPermissionText(
    const std::string& permission_text) const {
  Java_AssistantQrCodeImagePickerModelWrapper_setPermissionText(
      jni_env_, java_assistant_image_picker_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, permission_text));
}

void AssistantQrCodeImagePickerModelWrapper::SetPermissionButtonText(
    const std::string& permission_button_text) const {
  Java_AssistantQrCodeImagePickerModelWrapper_setPermissionButtonText(
      jni_env_, java_assistant_image_picker_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, permission_button_text));
}

void AssistantQrCodeImagePickerModelWrapper::SetOpenSettingsText(
    const std::string& open_settings_text) const {
  Java_AssistantQrCodeImagePickerModelWrapper_setOpenSettingsText(
      jni_env_, java_assistant_image_picker_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, open_settings_text));
}

void AssistantQrCodeImagePickerModelWrapper::SetOpenSettingsButtonText(
    const std::string& open_settings_button_text) const {
  Java_AssistantQrCodeImagePickerModelWrapper_setOpenSettingsButtonText(
      jni_env_, java_assistant_image_picker_model_wrapper_,
      ConvertUTF8ToJavaString(jni_env_, open_settings_button_text));
}
}  // namespace autofill_assistant