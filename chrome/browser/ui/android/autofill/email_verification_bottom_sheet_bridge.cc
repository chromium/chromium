// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/strings/grit/components_strings.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/EmailVerificationBottomSheetBridge_jni.h"

namespace autofill {

EmailVerificationBottomSheetBridge::EmailVerificationBottomSheetBridge(
    ui::WindowAndroid* window_android,
    TabModel* tab_model) {
  CHECK(window_android);
  CHECK(tab_model);
  java_email_verification_bottom_sheet_bridge_ =
      Java_EmailVerificationBottomSheetBridge_Constructor(
          base::android::AttachCurrentThread(), reinterpret_cast<int64_t>(this),
          window_android->GetJavaObject(), tab_model->GetJavaObject());
}

EmailVerificationBottomSheetBridge::~EmailVerificationBottomSheetBridge() {
  if (java_email_verification_bottom_sheet_bridge_) {
    Java_EmailVerificationBottomSheetBridge_destroy(
        base::android::AttachCurrentThread(),
        java_email_verification_bottom_sheet_bridge_);
  }
  RunCallback(AutofillClient::EmailVerificationPermissionUiStatus::
                  kViewDestroyedDirectly);
}

void EmailVerificationBottomSheetBridge::RequestShowContent(
    const std::u16string& issuer,
    const std::u16string& email,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)> callback) {
  if (callback_) {
    RunCallback(AutofillClient::EmailVerificationPermissionUiStatus::
                    kOverlappingPrompt);
  }
  callback_ = std::move(callback);
  if (!java_email_verification_bottom_sheet_bridge_) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_TITLE);
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_BODY, issuer, email);
  Java_EmailVerificationBottomSheetBridge_requestShowContent(
      env, java_email_verification_bottom_sheet_bridge_, title, description);
}

void EmailVerificationBottomSheetBridge::Hide() {
  if (java_email_verification_bottom_sheet_bridge_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_EmailVerificationBottomSheetBridge_hide(
        env, java_email_verification_bottom_sheet_bridge_);
  }
  RunCallback(
      AutofillClient::EmailVerificationPermissionUiStatus::kUserAborted);
}

EmailVerificationBottomSheetBridge::EmailVerificationBottomSheetBridge(
    base::android::ScopedJavaGlobalRef<jobject>
        java_email_verification_bottom_sheet_bridge)
    : java_email_verification_bottom_sheet_bridge_(
          java_email_verification_bottom_sheet_bridge) {}

void EmailVerificationBottomSheetBridge::OnUiShown(JNIEnv* env) {}

void EmailVerificationBottomSheetBridge::OnUiDecision(JNIEnv* env, int status) {
  RunCallback(
      static_cast<AutofillClient::EmailVerificationPermissionUiStatus>(status));
}

void EmailVerificationBottomSheetBridge::RunCallback(
    AutofillClient::EmailVerificationPermissionUiStatus status) {
  if (callback_) {
    std::move(callback_).Run(status);
  }
}

}  // namespace autofill

DEFINE_JNI(EmailVerificationBottomSheetBridge)
