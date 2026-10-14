// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/email_verification_bottom_sheet_bridge.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_deref.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/EmailVerificationBottomSheetBridge_jni.h"

namespace autofill {

EmailVerificationBottomSheetBridge::EmailVerificationBottomSheetBridge(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

EmailVerificationBottomSheetBridge::~EmailVerificationBottomSheetBridge() {
  if (java_email_verification_bottom_sheet_bridge_) {
    Java_EmailVerificationBottomSheetBridge_destroy(
        base::android::AttachCurrentThread(),
        java_email_verification_bottom_sheet_bridge_);
  }
  on_accepted_callback_.Reset();
  if (on_dismissed_callback_) {
    std::move(on_dismissed_callback_)
        .Run(AutofillClient::EmailVerificationPermissionUiStatus::
                 kViewDestroyedDirectly);
  }
}

void EmailVerificationBottomSheetBridge::RequestShowContent(
    const std::u16string& issuer,
    const std::u16string& email,
    base::OnceClosure on_accepted_callback,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)>
        on_dismissed_callback) {
  // If a bottom sheet is already active (either awaiting user decision or
  // displaying its loading spinner), reject the new request with
  // kOverlappingPrompt without disrupting the existing sheet.
  if (java_email_verification_bottom_sheet_bridge_) {
    if (on_dismissed_callback) {
      std::move(on_dismissed_callback)
          .Run(AutofillClient::EmailVerificationPermissionUiStatus::
                   kOverlappingPrompt);
    }
    return;
  }

  ui::WindowAndroid* window_android = web_contents_->GetTopLevelNativeWindow();
  if (!window_android) {
    if (on_dismissed_callback) {
      std::move(on_dismissed_callback)
          .Run(AutofillClient::EmailVerificationPermissionUiStatus::kOther);
    }
    return;
  }

  on_accepted_callback_ = std::move(on_accepted_callback);
  on_dismissed_callback_ = std::move(on_dismissed_callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  java_email_verification_bottom_sheet_bridge_ =
      Java_EmailVerificationBottomSheetBridge_Constructor(
          env, reinterpret_cast<intptr_t>(this),
          window_android->GetJavaObject());

  std::u16string title =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_TITLE);
  std::u16string description = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_EMAIL_VERIFIER_PROMPT_BODY, issuer, email);
  ShowJavaBottomSheet(title, description);
}

void EmailVerificationBottomSheetBridge::ShowJavaBottomSheet(
    const std::u16string& title,
    const std::u16string& description) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_EmailVerificationBottomSheetBridge_requestShowContent(
      env, java_email_verification_bottom_sheet_bridge_, title, description);
}

void EmailVerificationBottomSheetBridge::Hide() {
  if (java_email_verification_bottom_sheet_bridge_) {
    HideJavaBottomSheet();
  }
}

void EmailVerificationBottomSheetBridge::HideJavaBottomSheet() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_EmailVerificationBottomSheetBridge_hide(
      env, java_email_verification_bottom_sheet_bridge_);
}

void EmailVerificationBottomSheetBridge::OnUiShown(JNIEnv* env) {}

void EmailVerificationBottomSheetBridge::OnUiAccepted(JNIEnv* env) {
  if (on_accepted_callback_) {
    std::move(on_accepted_callback_).Run();
  }
}

void EmailVerificationBottomSheetBridge::OnUiDismissed(JNIEnv* env,
                                                       int reason) {
  java_email_verification_bottom_sheet_bridge_.Reset();
  on_accepted_callback_.Reset();
  if (on_dismissed_callback_) {
    std::move(on_dismissed_callback_)
        .Run(static_cast<AutofillClient::EmailVerificationPermissionUiStatus>(
            reason));
  }
}

}  // namespace autofill

DEFINE_JNI(EmailVerificationBottomSheetBridge)
