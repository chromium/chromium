// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/card_unmask_prompt_view_android.h"

#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/CardUnmaskBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

CardUnmaskPromptView* CreateCardUnmaskPromptView(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents) {
  return new CardUnmaskPromptViewAndroid(controller, web_contents);
}

CardUnmaskPromptViewAndroid::CardUnmaskPromptViewAndroid(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
}

CardUnmaskPromptViewAndroid::~CardUnmaskPromptViewAndroid() {
  if (controller_)
    controller_->OnUnmaskDialogClosed();
}

void CardUnmaskPromptViewAndroid::Show() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (view_android == nullptr || view_android->GetWindowAndroid() == nullptr)
    return;

  Java_CardUnmaskBridge_show(env, java_object,
                             view_android->GetWindowAndroid()->GetJavaObject());
}

void CardUnmaskPromptViewAndroid::Dismiss() {
  if (!java_object_internal_)
    return;
  Java_CardUnmaskBridge_dismiss(base::android::AttachCurrentThread(),
                                java_object_internal_);
}

bool CardUnmaskPromptViewAndroid::CheckUserInputValidity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const std::u16string& response) {
  return controller_->InputCvcIsValid(response);
}

void CardUnmaskPromptViewAndroid::OnUserInput(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              const std::u16string& cvc,
                                              const std::u16string& month,
                                              const std::u16string& year,
                                              jboolean enable_fido_auth,
                                              jboolean was_checkbox_visible) {
  controller_->OnUnmaskPromptAccepted(cvc, month, year, enable_fido_auth,
                                      was_checkbox_visible);
}

void CardUnmaskPromptViewAndroid::OnNewCardLinkClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  controller_->NewCardLinkClicked();
  Java_CardUnmaskBridge_update(env, java_object,
                               base::android::ConvertUTF16ToJavaString(
                                   env, controller_->GetWindowTitle()),
                               base::android::ConvertUTF16ToJavaString(
                                   env, controller_->GetInstructionsMessage()),
                               controller_->ShouldRequestExpirationDate());
}

int CardUnmaskPromptViewAndroid::GetExpectedCvcLength(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return controller_->GetExpectedCvcLength();
}

void CardUnmaskPromptViewAndroid::PromptDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  delete this;
}

void CardUnmaskPromptViewAndroid::ControllerGone() {
  controller_ = nullptr;
  Dismiss();
}

void CardUnmaskPromptViewAndroid::DisableAndWaitForVerification() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CardUnmaskBridge_disableAndWaitForVerification(env, java_object);
}

void CardUnmaskPromptViewAndroid::GotVerificationResult(
    const std::u16string& error_message,
    bool allow_retry) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object)
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> message;
  if (!error_message.empty())
      message = base::android::ConvertUTF16ToJavaString(env, error_message);

  Java_CardUnmaskBridge_verificationFinished(env, java_object, message,
                                             allow_retry);
}

base::android::ScopedJavaGlobalRef<jobject>
CardUnmaskPromptViewAndroid::GetOrCreateJavaObject() {
  if (java_object_internal_) {
    return java_object_internal_;
  }
  if (web_contents_->GetNativeView() == nullptr ||
      web_contents_->GetNativeView()->GetWindowAndroid() == nullptr) {
    return nullptr;  // No window attached (yet or anymore).
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  ScopedJavaLocalRef<jstring> dialog_title =
      base::android::ConvertUTF16ToJavaString(env,
                                              controller_->GetWindowTitle());
  ScopedJavaLocalRef<jstring> instructions =
      base::android::ConvertUTF16ToJavaString(
          env, controller_->GetInstructionsMessage());
  ScopedJavaLocalRef<jstring> card_name =
      base::android::ConvertUTF16ToJavaString(env, controller_->GetCardName());
  ScopedJavaLocalRef<jstring> card_last_four_digits =
      base::android::ConvertUTF16ToJavaString(
          env, controller_->GetCardLastFourDigits());
  ScopedJavaLocalRef<jstring> card_expiration =
      base::android::ConvertUTF16ToJavaString(env,
                                              controller_->GetCardExpiration());
  ScopedJavaLocalRef<jobject> card_art_url =
      url::GURLAndroid::FromNativeGURL(env, controller_->GetCardArtUrl());
  ScopedJavaLocalRef<jstring> confirm = base::android::ConvertUTF16ToJavaString(
      env, controller_->GetOkButtonLabel());
  ScopedJavaLocalRef<jstring> cvc_image_announcement =
      base::android::ConvertUTF16ToJavaString(
          env, controller_->GetCvcImageAnnouncement());

  return java_object_internal_ = Java_CardUnmaskBridge_create(
             env, reinterpret_cast<intptr_t>(this),
             Profile::FromBrowserContext(web_contents_->GetBrowserContext())
                 ->GetJavaObject(),
             dialog_title, instructions,
             ResourceMapper::MapToJavaDrawableId(
                 GetIconResourceID(controller_->GetCardIcon())),
             card_name, card_last_four_digits, card_expiration, card_art_url,
             confirm,
             ResourceMapper::MapToJavaDrawableId(controller_->GetCvcImageRid()),
             cvc_image_announcement,
             ResourceMapper::MapToJavaDrawableId(
                 controller_->GetGooglePayImageRid()),
             controller_->IsVirtualCard(),
             controller_->ShouldRequestExpirationDate(),
             controller_->ShouldOfferWebauthn(),
             controller_->GetWebauthnOfferStartState(),
             controller_->GetSuccessMessageDuration().InMilliseconds(),
             view_android->GetWindowAndroid()->GetJavaObject());
}

}  // namespace autofill
