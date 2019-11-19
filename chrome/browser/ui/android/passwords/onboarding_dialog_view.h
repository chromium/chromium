// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_ONBOARDING_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_ONBOARDING_DIALOG_VIEW_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_onboarding.h"

class ChromePasswordManagerClient;

namespace password_manager {
class PasswordFormManagerForUI;
}  // namespace password_manager

// The class connecting the native code to the UI for the password manager
// onboarding experience. This class will delete itself once the dialog is
// dismissed in any way (e.g. via button click, back press, tab being destroyed,
// etc.).
class OnboardingDialogView {
 public:
  explicit OnboardingDialogView(
      ChromePasswordManagerClient* client,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);

  ~OnboardingDialogView();

  // Called to show the onboarding dialog.
  // Sets the |kPasswordManagerOnboardingState| pref to |kShown|.
  void Show();

  // Called from Java via JNI when the positive button is pressed,
  // e.g. "Continue" or "Got it". Prompts the user to save their password.
  void OnboardingAccepted(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Called from Java via JNI when the negative button is pressed,
  // e.g. "Cancel".
  void OnboardingRejected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Called from Java via JNI when the dialog is dismissed in any other way,
  // e.g. pressing the back button or navigating to another tab.
  void OnboardingAborted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

 private:
  // Records the reaction to the onboarding and deletes this class.
  void DismissWithReasonAndDelete(
      password_manager::metrics_util::OnboardingUIDismissalReason reason);

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Form that is needed to prompt the user to save their password
  // after the onboarding was shown.
  std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save_;

  // Needed for prompting the user the save their password, as well as
  // retrieving web_contents and setting prefs.
  ChromePasswordManagerClient* client_;

  std::unique_ptr<password_manager::SavingFlowMetricsRecorder>
      saving_flow_recorder_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingDialogView);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_ONBOARDING_DIALOG_VIEW_H_
