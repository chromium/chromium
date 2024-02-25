// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTHENTICATOR_SELECTION_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTHENTICATOR_SELECTION_DIALOG_VIEW_ANDROID_H_

#include <jni.h>
#include <stddef.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog.h"
#include "ui/android/window_android.h"

namespace autofill {
class CardUnmaskAuthenticationSelectionDialogController;
struct CardUnmaskChallengeOption;

// Android implementation of the CardUnmaskAuthenticationSelectionDialog.
// `CardUnmaskAuthenticationSelectionDialogControllerImpl` holds a pointer to
// this View. The View is expected to call "delete this" on itself upon
// dismissal.
class AuthenticatorSelectionDialogViewAndroid
    : public CardUnmaskAuthenticationSelectionDialog {
 public:
  explicit AuthenticatorSelectionDialogViewAndroid(
      CardUnmaskAuthenticationSelectionDialogController* controller);
  virtual ~AuthenticatorSelectionDialogViewAndroid();

  AuthenticatorSelectionDialogViewAndroid(
      const AuthenticatorSelectionDialogViewAndroid&) = delete;
  AuthenticatorSelectionDialogViewAndroid& operator=(
      const AuthenticatorSelectionDialogViewAndroid&) = delete;

  // CardUnmaskAuthenticationSelectionDialog.
  void Dismiss(bool user_closed_dialog, bool server_success) override;
  void UpdateContent() override;

  // Called by the Java code when an Authenticator selection is made.
  void OnOptionSelected(JNIEnv* env,
                        const base::android::JavaParamRef<jstring>&
                            authenticator_option_identifier);

  // Called by the Java code when the authenticatior selection dialog is
  // dismissed.
  void OnDismissed(JNIEnv* env);

  // Show the dialog view.
  bool ShowDialog(ui::WindowAndroid* window_android);

 private:
  raw_ptr<CardUnmaskAuthenticationSelectionDialogController> controller_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Uses JNI to create and return Java AuthenticatorOptions given a vector of
  // CardUnmaskChallengeOptions.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaAuthenticatorOptions(
      JNIEnv* env,
      const std::vector<CardUnmaskChallengeOption>& options);

  // Uses JNI to create and add AuthenticatorOptions to a Java List.
  void CreateJavaAuthenticatorOptionAndAddToList(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jobject> jlist,
      const CardUnmaskChallengeOption& option);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTHENTICATOR_SELECTION_DIALOG_VIEW_ANDROID_H_
