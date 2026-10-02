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
#include "base/functional/callback_helpers.h"
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
  // `on_accepted_callback` is executed if the user clicks the "Verify" button.
  // `on_dismissed_callback` is executed when the bottom sheet is dismissed or
  // closed, with the dismissal reason.
  // Overridden in tests.
  virtual void RequestShowContent(
      const std::u16string& issuer,
      const std::u16string& email,
      base::OnceClosure on_accepted_callback,
      base::OnceCallback<
          void(AutofillClient::EmailVerificationPermissionUiStatus)>
          on_dismissed_callback);

  // Hides the email verification bottom sheet.
  virtual void Hide();

  // -- JNI calls bridged from Java --
  // Called when the UI is shown.
  void OnUiShown(JNIEnv* env);
  // Called when the user clicks the accept/verify button.
  void OnUiAccepted(JNIEnv* env);
  // Called when the UI is dismissed/hidden.
  void OnUiDismissed(JNIEnv* env, int reason);

 protected:
  // Used in tests to inject dependencies.
  explicit EmailVerificationBottomSheetBridge(
      base::android::ScopedJavaGlobalRef<jobject>
          java_email_verification_bottom_sheet_bridge);

 private:
  base::android::ScopedJavaGlobalRef<jobject>
      java_email_verification_bottom_sheet_bridge_;
  base::OnceClosure on_accepted_callback_;
  base::OnceCallback<void(AutofillClient::EmailVerificationPermissionUiStatus)>
      on_dismissed_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_
