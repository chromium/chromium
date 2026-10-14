// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Bridge class owned by ChromeAutofillClient providing an entry point
// to trigger the email verification bottom sheet on Android.
class EmailVerificationBottomSheetBridge {
 public:
  explicit EmailVerificationBottomSheetBridge(
      content::WebContents* web_contents);

  EmailVerificationBottomSheetBridge(
      const EmailVerificationBottomSheetBridge&) = delete;
  EmailVerificationBottomSheetBridge& operator=(
      const EmailVerificationBottomSheetBridge&) = delete;

  virtual ~EmailVerificationBottomSheetBridge();

  // Requests to show the email verification bottom sheet.
  // `on_accepted_callback` is executed if the user clicks the "Verify" button.
  // `on_dismissed_callback` is executed when the bottom sheet is dismissed or
  // closed, with the dismissal reason.
  void RequestShowContent(
      const std::u16string& issuer,
      const std::u16string& email,
      base::OnceClosure on_accepted_callback,
      base::OnceCallback<
          void(AutofillClient::EmailVerificationPermissionUiStatus)>
          on_dismissed_callback);

  // Hides the email verification bottom sheet.
  void Hide();

  // -- JNI calls bridged from Java --
  // Called when the UI is shown.
  void OnUiShown(JNIEnv* env);
  // Called when the user clicks the accept/verify button.
  void OnUiAccepted(JNIEnv* env);
  // Called when the UI is dismissed/hidden.
  void OnUiDismissed(JNIEnv* env, int reason);

 protected:
  // TODO(crbug.com/555673134): Avoid overriding protected methods in tests.
  // Requests the Java bridge to show the bottom sheet. Overridden in unit
  // tests to bypass JNI calls that require an attached BottomSheetController.
  virtual void ShowJavaBottomSheet(const std::u16string& title,
                                   const std::u16string& description);
  // Requests the Java bridge to hide the bottom sheet. Overridden in unit
  // tests to bypass JNI calls that require an attached BottomSheetController.
  virtual void HideJavaBottomSheet();

 private:
  const raw_ref<content::WebContents> web_contents_;
  base::android::ScopedJavaGlobalRef<jobject>
      java_email_verification_bottom_sheet_bridge_;

  base::OnceClosure on_accepted_callback_;
  base::OnceCallback<void(AutofillClient::EmailVerificationPermissionUiStatus)>
      on_dismissed_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_EMAIL_VERIFICATION_BOTTOM_SHEET_BRIDGE_H_
