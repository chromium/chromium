// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_attribution_verification_sheet_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check_deref.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillAiAttributionVerificationSheetBridge_jni.h"

namespace autofill {

AutofillAiAttributionVerificationSheetBridge::
    AutofillAiAttributionVerificationSheetBridge(
        content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

AutofillAiAttributionVerificationSheetBridge::
    ~AutofillAiAttributionVerificationSheetBridge() {
  ResetJavaBridge();
  if (decision_cb_) {
    std::move(decision_cb_)
        .Run(AutofillClient::AutofillAiVerificationDecision::kDismiss);
  }
}

void AutofillAiAttributionVerificationSheetBridge::RequestShowContent(
    AutofillClient::AutofillAiVerificationDecisionCallback decision_cb) {
  ResetJavaBridge();
  if (decision_cb_) {
    std::move(decision_cb_)
        .Run(AutofillClient::AutofillAiVerificationDecision::kDismiss);
  }

  ui::WindowAndroid* window_android = web_contents_->GetTopLevelNativeWindow();
  if (!window_android) {
    if (decision_cb) {
      std::move(decision_cb)
          .Run(AutofillClient::AutofillAiVerificationDecision::kDismiss);
    }
    return;
  }

  decision_cb_ = std::move(decision_cb);
  JNIEnv* env = base::android::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> bridge =
      Java_AutofillAiAttributionVerificationSheetBridge_Constructor(
          env, jni_zero::MakeRaw(this), window_android);
  java_bridge_.Reset(bridge);
  Java_AutofillAiAttributionVerificationSheetBridge_requestShowContent(env,
                                                                       bridge);
}

void AutofillAiAttributionVerificationSheetBridge::OnDismissed() {
  ResetJavaBridge();
  if (decision_cb_) {
    std::move(decision_cb_)
        .Run(AutofillClient::AutofillAiVerificationDecision::kDismiss);
  }
}

void AutofillAiAttributionVerificationSheetBridge::ResetJavaBridge() {
  if (!java_bridge_) {
    return;
  }
  Java_AutofillAiAttributionVerificationSheetBridge_destroy(
      base::android::AttachCurrentThread(), java_bridge_);
  java_bridge_.Reset();
}

}  // namespace autofill

DEFINE_JNI(AutofillAiAttributionVerificationSheetBridge)
