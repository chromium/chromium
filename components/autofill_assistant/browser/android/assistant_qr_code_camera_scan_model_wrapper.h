// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_QR_CODE_CAMERA_SCAN_MODEL_WRAPPER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_QR_CODE_CAMERA_SCAN_MODEL_WRAPPER_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/android/assistant_qr_code_native_delegate.h"

namespace autofill_assistant {

// C++ equivalent to java-side |AssistantQrCodeCameraScanModelWrapper|.
class AssistantQrCodeCameraScanModelWrapper {
 public:
  explicit AssistantQrCodeCameraScanModelWrapper();
  ~AssistantQrCodeCameraScanModelWrapper();

  base::android::ScopedJavaGlobalRef<jobject> GetModel() const;
  void SetDelegate(const base::android::ScopedJavaGlobalRef<jobject>&
                       java_qr_code_native_delegate) const;
  void SetToolbarTitle(const std::string& title_text) const;
  void SetPermissionText(const std::string& permission_text) const;
  void SetPermissionButtonText(const std::string& permission_button_text) const;
  void SetOpenSettingsText(const std::string& open_settings_text) const;
  void SetOpenSettingsButtonText(
      const std::string& open_settings_button_text) const;
  void SetCameraPreviewInstructionText(
      const std::string& camera_preview_instruction_text) const;
  void SetCameraPreviewSecurityText(
      const std::string& camera_preview_security_text) const;

 private:
  JNIEnv* jni_env_;
  // Java-side AssistantQrCodeCameraScanModelWrapper object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_camera_scan_model_wrapper_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_QR_CODE_CAMERA_SCAN_MODEL_WRAPPER_H_