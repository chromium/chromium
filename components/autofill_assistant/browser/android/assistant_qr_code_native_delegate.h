// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_QR_CODE_NATIVE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_QR_CODE_NATIVE_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace autofill_assistant {

class UiControllerAndroid;

// Delegate class for the QR Code. Receives events from the Java UI and
// forwards them to the ui controller. This is the JNI bridge to
// |AssistantQrCodeNativeDelegate.java|.
class AssistantQrCodeNativeDelegate {
 public:
  // Note: |ui_controller| must outlive this instance.
  explicit AssistantQrCodeNativeDelegate(UiControllerAndroid* ui_controller);
  ~AssistantQrCodeNativeDelegate();

  // This method is invoked from the java-side when QR code scanning is
  // finished successfully.
  void OnScanResult(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller,
                    const base::android::JavaParamRef<jstring>& jvalue);

  // This method is invoked from the java-side when QR code scanning is
  // cancelled by the user.
  void OnScanCancelled(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jcaller);

  // This method is invoked from the java-side when QR code scanning fails to
  // give any output.
  void OnScanFailure(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller);

  // This method is invoked from the java-side when QR code scanning is
  // interrupted by a camera error.
  void OnCameraError(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject() const;

 private:
  raw_ptr<UiControllerAndroid> ui_controller_;

  // Java-side AssistantQrCodeNativeDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_qr_code_native_delegate_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_QR_CODE_NATIVE_DELEGATE_H_
