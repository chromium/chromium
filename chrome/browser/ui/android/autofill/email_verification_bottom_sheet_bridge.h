// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

class TabModel;

namespace ui {
class WindowAndroid;
}

namespace autofill {

// Bridge class owned by ChromeAutofillClient providing an entry point
// to trigger the email verification bottom sheet on Android.
class EmailVerificationBottomSheetBridge {
 public:
  // The window and tab model must not be null.
  EmailVerificationBottomSheetBridge(ui::WindowAndroid* window_android,
                                     TabModel* tab_model);

  EmailVerificationBottomSheetBridge(
      const EmailVerificationBottomSheetBridge&) = delete;
  EmailVerificationBottomSheetBridge& operator=(
      const EmailVerificationBottomSheetBridge&) = delete;

  virtual ~EmailVerificationBottomSheetBridge();

  // Requests to show the email verification bottom sheet.
  // Overridden in tests.
  virtual void RequestShowContent(
      const std::u16string& issuer,
      const std::u16string& email,
      base::OnceCallback<
          void(AutofillClient::EmailVerificationPermissionUiStatus)> callback);

  // Hides the email verification bottom sheet.
  virtual void Hide();

  // -- JNI calls bridged from Java --
  // Called when the UI is shown.
  void OnUiShown(JNIEnv* env);
  // Called when a UI decision is made with the corresponding status code.
  void OnUiDecision(JNIEnv* env, int status);

 protected:
  // Used in tests to inject dependencies.
  explicit EmailVerificationBottomSheetBridge(
      base::android::ScopedJavaGlobalRef<jobject>
          java_email_verification_bottom_sheet_bridge);

 private:
  void RunCallback(AutofillClient::EmailVerificationPermissionUiStatus status);

  base::android::ScopedJavaGlobalRef<jobject>
      java_email_verification_bottom_sheet_bridge_;
  base::OnceCallback<void(AutofillClient::EmailVerificationPermissionUiStatus)>
      callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_
