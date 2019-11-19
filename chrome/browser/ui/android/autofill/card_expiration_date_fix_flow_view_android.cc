// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/card_expiration_date_fix_flow_view_android.h"

#include "chrome/android/chrome_jni_headers/AutofillExpirationDateFixFlowBridge_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

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
    const JavaParamRef<jstring>& month,
    const JavaParamRef<jstring>& year) {
  controller_->OnAccepted(base::android::ConvertJavaStringToUTF16(env, month),
                          base::android::ConvertJavaStringToUTF16(env, year));
}

void CardExpirationDateFixFlowViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  controller_->OnDismissed();
  delete this;
}

void CardExpirationDateFixFlowViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> dialog_title =
      base::android::ConvertUTF16ToJavaString(env, controller_->GetTitleText());

  ScopedJavaLocalRef<jstring> confirm = base::android::ConvertUTF16ToJavaString(
      env, controller_->GetSaveButtonLabel());

  ScopedJavaLocalRef<jstring> card_label =
      base::android::ConvertUTF16ToJavaString(env, controller_->GetCardLabel());

  java_object_.Reset(Java_AutofillExpirationDateFixFlowBridge_create(
      env, reinterpret_cast<intptr_t>(this), dialog_title, confirm,
      ResourceMapper::MapFromChromiumId(controller_->GetIconId()), card_label));

  Java_AutofillExpirationDateFixFlowBridge_show(
      env, java_object_,
      web_contents_->GetTopLevelNativeWindow()->GetJavaObject());
}

void CardExpirationDateFixFlowViewAndroid::ControllerGone() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillExpirationDateFixFlowBridge_dismiss(env, java_object_);
}

CardExpirationDateFixFlowViewAndroid::~CardExpirationDateFixFlowViewAndroid() {
  if (controller_)
    controller_->OnDialogClosed();
}

}  // namespace autofill
