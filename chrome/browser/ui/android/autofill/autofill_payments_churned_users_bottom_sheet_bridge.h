// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_CHURNED_USERS_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_CHURNED_USERS_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/ui/payments/payments_churned_users_ui_delegate.h"

namespace ui {
class WindowAndroid;
}

namespace autofill {

// Bridge class to trigger the Payments Churned Users bottom sheet on Android.
class AutofillPaymentsChurnedUsersBottomSheetBridge {
 public:
  explicit AutofillPaymentsChurnedUsersBottomSheetBridge(
      ui::WindowAndroid* window_android);

  AutofillPaymentsChurnedUsersBottomSheetBridge(
      const AutofillPaymentsChurnedUsersBottomSheetBridge&) = delete;
  AutofillPaymentsChurnedUsersBottomSheetBridge& operator=(
      const AutofillPaymentsChurnedUsersBottomSheetBridge&) = delete;

  virtual ~AutofillPaymentsChurnedUsersBottomSheetBridge();

  // Requests to show the bottom sheet for the given `treatment_arm`.
  virtual void RequestShowContent(
      AutofillEnableResurrectingPaymentsUsersTreatmentArm treatment_arm,
      base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback,
      base::OnceClosure show_confirmation_callback);

  // -- JNI calls bridged from Java --
  void OnUiAccepted(JNIEnv* env);
  void OnShowConfirmation(JNIEnv* env);
  void OnUiCanceled(JNIEnv* env);
  void OnUiDismissed(JNIEnv* env);
  void OnUiNotShown(JNIEnv* env);

 private:
  friend class AutofillPaymentsChurnedUsersBottomSheetBridgeTestApi;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::OnceCallback<void(PaymentsUiClosedReason)> closed_callback_;
  base::OnceClosure show_confirmation_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PAYMENTS_CHURNED_USERS_BOTTOM_SHEET_BRIDGE_H_
