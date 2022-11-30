// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_overlay_delegate.h"

#include "components/autofill_assistant/android/jni_headers/AssistantOverlayDelegate_jni.h"
#include "components/autofill_assistant/browser/android/ui_controller_android.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantOverlayDelegate::AssistantOverlayDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_overlay_delegate_ = Java_AssistantOverlayDelegate_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantOverlayDelegate::~AssistantOverlayDelegate() {
  Java_AssistantOverlayDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_overlay_delegate_);
}

void AssistantOverlayDelegate::OnUnexpectedTaps(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  ui_controller_->OnUnexpectedTaps();
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantOverlayDelegate::GetJavaObject() {
  return java_assistant_overlay_delegate_;
}

}  // namespace autofill_assistant
