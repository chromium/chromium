// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_payments_churned_users_bottom_sheet_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that declare env/types.
#include "chrome/browser/autofill/android/jni_headers/AutofillPaymentsChurnedUsersBottomSheetBridge_jni.h"

namespace autofill {

AutofillPaymentsChurnedUsersBottomSheetBridge::
    AutofillPaymentsChurnedUsersBottomSheetBridge(
        ui::WindowAndroid* window_android) {
  if (window_android && window_android->GetJavaObject()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_object_ =
        Java_AutofillPaymentsChurnedUsersBottomSheetBridge_Constructor(
            env, reinterpret_cast<intptr_t>(this),
            window_android->GetJavaObject());
  }
}

AutofillPaymentsChurnedUsersBottomSheetBridge::
    ~AutofillPaymentsChurnedUsersBottomSheetBridge() {
  if (java_object_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AutofillPaymentsChurnedUsersBottomSheetBridge_destroy(env,
                                                               java_object_);
  }
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kUnknown);
  }
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::RequestShowContent(
    AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm,
    base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback,
    base::OnceClosure show_confirmation_callback) {
  closed_callback_ = std::move(closed_callback);
  show_confirmation_callback_ = std::move(show_confirmation_callback);
  if (!java_object_) {
    OnUiNotShown(nullptr);
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPaymentsChurnedUsersBottomSheetBridge_requestShowContent(
      env, java_object_, static_cast<int>(treatment_arm));
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::OnUiAccepted(JNIEnv* env) {
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kAccepted);
  }
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::OnShowConfirmation(
    JNIEnv* env) {
  if (show_confirmation_callback_) {
    std::move(show_confirmation_callback_).Run();
  }
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::OnUiCanceled(JNIEnv* env) {
  show_confirmation_callback_.Reset();
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kCancelled);
  }
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::OnUiDismissed(JNIEnv* env) {
  show_confirmation_callback_.Reset();
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kNotInteracted);
  }
}

void AutofillPaymentsChurnedUsersBottomSheetBridge::OnUiNotShown(JNIEnv* env) {
  show_confirmation_callback_.Reset();
  if (closed_callback_) {
    std::move(closed_callback_).Run(PaymentsUiClosedReason::kUnknown);
  }
}

}  // namespace autofill

DEFINE_JNI(AutofillPaymentsChurnedUsersBottomSheetBridge)
