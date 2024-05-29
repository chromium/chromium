// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/card_expiration_date_fix_flow_view_android.h"

#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillExpirationDateFixFlowBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

CardExpirationDateFixFlowViewAndroid::CardExpirationDateFixFlowViewAndroid(
    CardExpirationDateFixFlowController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

void CardExpirationDateFixFlowViewAndroid::OnUserAccept(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const std::u16string& month,
    const std::u16string& year) {
  controller_->OnAccepted(month, year);
}

void CardExpirationDateFixFlowViewAndroid::OnUserDismiss(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  controller_->OnDismissed();
}

void CardExpirationDateFixFlowViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  delete this;
}

void CardExpirationDateFixFlowViewAndroid::Show() {
  if (!web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    return;  // No window attached (yet or anymore).
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  java_object_.Reset(Java_AutofillExpirationDateFixFlowBridge_create(
      env, reinterpret_cast<intptr_t>(this), controller_->GetTitleText(),
      controller_->GetSaveButtonLabel(),
      ResourceMapper::MapToJavaDrawableId(controller_->GetIconId()),
      controller_->GetCardLabel()));

  Java_AutofillExpirationDateFixFlowBridge_show(
      env, java_object_,
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject());
}

void CardExpirationDateFixFlowViewAndroid::ControllerGone() {
  controller_ = nullptr;
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutofillExpirationDateFixFlowBridge_dismiss(env, java_object_);
  }
}

CardExpirationDateFixFlowViewAndroid::~CardExpirationDateFixFlowViewAndroid() {
  if (controller_)
    controller_->OnDialogClosed();
}

}  // namespace autofill
