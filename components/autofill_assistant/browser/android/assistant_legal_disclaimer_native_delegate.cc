// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_legal_disclaimer_native_delegate.h"

#include "components/autofill_assistant/android/jni_headers/AssistantLegalDisclaimerNativeDelegate_jni.h"
#include "components/autofill_assistant/browser/android/ui_controller_android.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantLegalDisclaimerNativeDelegate::AssistantLegalDisclaimerNativeDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_native_delegate_ =
      Java_AssistantLegalDisclaimerNativeDelegate_Constructor(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantLegalDisclaimerNativeDelegate::
    ~AssistantLegalDisclaimerNativeDelegate() {
  Java_AssistantLegalDisclaimerNativeDelegate_clearNativePtr(
      AttachCurrentThread(), java_native_delegate_);
}

void AssistantLegalDisclaimerNativeDelegate::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint link) {
  ui_controller_->OnLegalDisclaimerLinkClicked(link);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantLegalDisclaimerNativeDelegate::GetJavaObject() {
  return java_native_delegate_;
}
}  // namespace autofill_assistant
