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
  if (on_accepted_callback_ || on_dismissed_callback_) {
    if (on_dismissed_callback) {
      std::move(on_dismissed_callback)
          .Run(AutofillClient::EmailVerificationPermissionUiStatus::
                   kOverlappingPrompt);
    }
    return;
  }
  on_accepted_callback_ = std::move(on_accepted_callback);
  on_dismissed_callback_ = std::move(on_dismissed_callback);
  // TODO(crbug.com/496177772): Use `java_email_verification_bottom_sheet_bridge_`
  // as the single source of truth once initialized in `RequestShowContent()`.
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
  } else {
    // TODO(crbug.com/496177772): Remove this branch when
    // `java_email_verification_bottom_sheet_bridge_` is initialized in
    // `RequestShowContent()`.
    auto status =
        on_accepted_callback_
            ? AutofillClient::EmailVerificationPermissionUiStatus::kOther
            : AutofillClient::EmailVerificationPermissionUiStatus::kAllowed;
    OnUiDismissed(/*env=*/nullptr, static_cast<int>(status));
  }
}

EmailVerificationBottomSheetBridge::EmailVerificationBottomSheetBridge(
    base::android::ScopedJavaGlobalRef<jobject>
        java_email_verification_bottom_sheet_bridge)
    : java_email_verification_bottom_sheet_bridge_(
          java_email_verification_bottom_sheet_bridge) {}

void EmailVerificationBottomSheetBridge::OnUiShown(JNIEnv* env) {}

void EmailVerificationBottomSheetBridge::OnUiAccepted(JNIEnv* env) {
  if (on_accepted_callback_) {
    std::move(on_accepted_callback_).Run();
  }
}

void EmailVerificationBottomSheetBridge::OnUiDismissed(JNIEnv* env,
                                                       int reason) {
  on_accepted_callback_.Reset();
  if (on_dismissed_callback_) {
    std::move(on_dismissed_callback_)
        .Run(static_cast<AutofillClient::EmailVerificationPermissionUiStatus>(
            reason));
  }
}

}  // namespace autofill

DEFINE_JNI(EmailVerificationBottomSheetBridge)
