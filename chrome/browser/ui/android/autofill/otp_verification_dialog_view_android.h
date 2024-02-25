// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_OTP_VERIFICATION_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_OTP_VERIFICATION_DIALOG_VIEW_ANDROID_H_

#include "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_view.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;

namespace autofill {

class CardUnmaskOtpInputDialogController;

// Android implementation of the OtpVerificationDialogView. This view deletes
// itself when either the dialog is dismissed or its corresponding controller is
// destroyed.
class OtpVerificationDialogViewAndroid : public CardUnmaskOtpInputDialogView {
 public:
  explicit OtpVerificationDialogViewAndroid(
      base::WeakPtr<CardUnmaskOtpInputDialogController> controller);
  OtpVerificationDialogViewAndroid(const OtpVerificationDialogViewAndroid&) =
      delete;
  OtpVerificationDialogViewAndroid& operator=(
      const OtpVerificationDialogViewAndroid&) = delete;
  ~OtpVerificationDialogViewAndroid() override;

  // CardUnmaskOtpInputDialogView.
  void ShowPendingState() override;
  void ShowInvalidState(const std::u16string& invalid_label_text) override;
  void Dismiss(bool show_confirmation_before_closing,
               bool user_closed_dialog) override;
  base::WeakPtr<CardUnmaskOtpInputDialogView> GetWeakPtr() override;

  // Called by the Java code when the error dialog is dismissed.
  void OnDialogDismissed(JNIEnv* env);
  // Called by the Java code when the user submits an OTP.
  void OnConfirm(JNIEnv* env, const JavaParamRef<jstring>& otp);
  // Called by the Java code when the user requests for a new OTP.
  void OnNewOtpRequested(JNIEnv* env);

  bool ShowDialog(ui::WindowAndroid* windowAndroid);

 private:
  void ShowConfirmationAndDismissDialog(std::u16string confirmation_message);
  base::WeakPtr<CardUnmaskOtpInputDialogController> controller_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::WeakPtrFactory<OtpVerificationDialogViewAndroid> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_OTP_VERIFICATION_DIALOG_VIEW_ANDROID_H_
