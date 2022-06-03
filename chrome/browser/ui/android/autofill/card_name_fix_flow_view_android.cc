// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/card_name_fix_flow_view_android.h"

#include "chrome/android/chrome_jni_headers/AutofillNameFixFlowBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

CardNameFixFlowViewAndroid::CardNameFixFlowViewAndroid(
    CardNameFixFlowController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

void CardNameFixFlowViewAndroid::OnUserAccept(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& name) {
  controller_->OnNameAccepted(
      base::android::ConvertJavaStringToUTF16(env, name));
}

void CardNameFixFlowViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  delete this;
}

void CardNameFixFlowViewAndroid::Show() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;

  java_object_.Reset(java_object);

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();

  Java_AutofillNameFixFlowBridge_show(
      env, java_object_, view_android->GetWindowAndroid()->GetJavaObject());
}

void CardNameFixFlowViewAndroid::ControllerGone() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (java_object_internal_) {
    // Don't create an object just for dismiss.
    Java_AutofillNameFixFlowBridge_dismiss(env, java_object_internal_);
  }
}

CardNameFixFlowViewAndroid::~CardNameFixFlowViewAndroid() {
  if (controller_)
    controller_->OnConfirmNameDialogClosed();
}

base::android::ScopedJavaGlobalRef<jobject>
CardNameFixFlowViewAndroid::GetOrCreateJavaObject() {
  if (java_object_internal_)
    return java_object_internal_;

  if (web_contents_->GetNativeView() == nullptr ||
      web_contents_->GetNativeView()->GetWindowAndroid() == nullptr)
    return nullptr;  // No window attached (yet or anymore).

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();

  ScopedJavaLocalRef<jstring> dialog_title =
      base::android::ConvertUTF16ToJavaString(env, controller_->GetTitleText());

  ScopedJavaLocalRef<jstring> inferred_name =
      base::android::ConvertUTF16ToJavaString(
          env, controller_->GetInferredCardholderName());

  ScopedJavaLocalRef<jstring> confirm = base::android::ConvertUTF16ToJavaString(
      env, controller_->GetSaveButtonLabel());

  return java_object_internal_ = Java_AutofillNameFixFlowBridge_create(
             env, reinterpret_cast<intptr_t>(this), dialog_title, inferred_name,
             confirm,
             ResourceMapper::MapToJavaDrawableId(controller_->GetIconId()),
             view_android->GetWindowAndroid()->GetJavaObject());
}

}  // namespace autofill
