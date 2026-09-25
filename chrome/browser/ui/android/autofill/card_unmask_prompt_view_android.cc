// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/card_unmask_prompt_view_android.h"

#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/create_card_unmask_prompt_view.h"
#include "components/autofill/core/browser/ui/autofill_resource_util.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/android/chrome_jni_headers/CardUnmaskBridge_jni.h"

namespace autofill {

CardUnmaskPromptView* CreateCardUnmaskPromptView(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents) {
  return new CardUnmaskPromptViewAndroid(controller, web_contents);
}

CardUnmaskPromptViewAndroid::CardUnmaskPromptViewAndroid(
    CardUnmaskPromptController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

CardUnmaskPromptViewAndroid::~CardUnmaskPromptViewAndroid() {
  if (controller_) {
    controller_->OnUnmaskDialogClosed();
  }
}

void CardUnmaskPromptViewAndroid::Show() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents_->GetNativeView();
  if (view_android == nullptr || view_android->GetWindowAndroid() == nullptr) {
    return;
  }

  Java_CardUnmaskBridge_show(env, java_object,
                             view_android->GetWindowAndroid());
}

void CardUnmaskPromptViewAndroid::Dismiss() {
  if (!java_object_internal_) {
    return;
  }
  Java_CardUnmaskBridge_dismiss(base::android::AttachCurrentThread(),
                                java_object_internal_);
}

bool CardUnmaskPromptViewAndroid::CheckUserInputValidity(
    const std::u16string& response) {
  return controller_->InputCvcIsValid(response);
}

void CardUnmaskPromptViewAndroid::OnUserInput(const std::u16string& cvc,
                                              const std::u16string& month,
                                              const std::u16string& year,
                                              bool enable_fido_auth,
                                              bool was_checkbox_visible) {
  controller_->OnUnmaskPromptAccepted(cvc, month, year, enable_fido_auth,
                                      was_checkbox_visible);
}

void CardUnmaskPromptViewAndroid::OnNewCardLinkClicked() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object) {
    return;
  }
  controller_->NewCardLinkClicked();
  Java_CardUnmaskBridge_update(base::android::AttachCurrentThread(),
                               java_object, controller_->GetWindowTitle(),
                               controller_->GetInstructionsMessage(),
                               controller_->ShouldRequestExpirationDate());
}

int CardUnmaskPromptViewAndroid::GetExpectedCvcLength() {
  return controller_->GetExpectedCvcLength();
}

void CardUnmaskPromptViewAndroid::PromptDismissed() {
  delete this;
}

void CardUnmaskPromptViewAndroid::ControllerGone() {
  controller_ = nullptr;
  Dismiss();
}

void CardUnmaskPromptViewAndroid::DisableAndWaitForVerification() {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CardUnmaskBridge_disableAndWaitForVerification(env, java_object);
}

void CardUnmaskPromptViewAndroid::GotVerificationResult(
    const std::u16string& error_message,
    bool allow_retry) {
  auto java_object = GetOrCreateJavaObject();
  if (!java_object) {
    return;
  }
  Java_CardUnmaskBridge_verificationFinished(
      base::android::AttachCurrentThread(), java_object, error_message,
      allow_retry);
}

jni_zero::ScopedJavaGlobalRef<jobject>
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

  return java_object_internal_ = Java_CardUnmaskBridge_create(
             env, reinterpret_cast<intptr_t>(this),
             Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
             controller_->GetWindowTitle(),
             controller_->GetInstructionsMessage(),
             ResourceMapper::MapToJavaDrawableId(
                 GetIconResourceID(controller_->GetCardIcon())),
             controller_->GetCardName(), controller_->GetCardLastFourDigits(),
             controller_->GetCardExpiration(), controller_->GetCardArtUrl(),
             controller_->GetOkButtonLabel(),
             ResourceMapper::MapToJavaDrawableId(controller_->GetCvcImageRid()),
             controller_->GetCvcImageAnnouncement(),
             ResourceMapper::MapToJavaDrawableId(
                 controller_->GetGooglePayImageRid()),
             controller_->IsVirtualCard(),
             controller_->ShouldRequestExpirationDate(),
             controller_->ShouldOfferWebauthn(),
             controller_->GetWebauthnOfferStartState(),
             controller_->GetSuccessMessageDuration().InMilliseconds(),
             view_android->GetWindowAndroid());
}

}  // namespace autofill

DEFINE_JNI(CardUnmaskBridge)
