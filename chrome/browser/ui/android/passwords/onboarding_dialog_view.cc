// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/passwords/onboarding_dialog_view.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "chrome/android/chrome_jni_headers/OnboardingDialogBridge_jni.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_onboarding.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Story centered on your password being available on multiple devices.
constexpr char kStoryB[] = "access";
// Story centered on password safety and leak detection.
constexpr char kStoryC[] = "safety";

// Retrieve the title and explanation strings that will be used for the
// dialog from the |story| parameter of the |kPasswordManagerOnboardingAndroid|
// feature, which will be provided when running experiments.
// The first story is run if the parameter isn't provided.
std::pair<base::string16, base::string16> GetOnboardingTitleAndDetails() {
  std::string story = base::GetFieldTrialParamValueByFeature(
      password_manager::features::kPasswordManagerOnboardingAndroid, "story");

  // By default the story centered on not having to remember your password is
  // shown.
  base::string16 onboarding_title =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_TITLE_A);
  base::string16 onboarding_details =
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_DETAILS_A);

  if (story == kStoryB) {
    onboarding_title =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_TITLE_B);
    onboarding_details =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_DETAILS_B);
  } else if (story == kStoryC) {
    onboarding_title =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_TITLE_C);
    onboarding_details =
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ONBOARDING_DETAILS_C);
  }

  return {onboarding_title, onboarding_details};
}

}  // namespace

OnboardingDialogView::OnboardingDialogView(
    ChromePasswordManagerClient* client,
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save)
    : form_to_save_(std::move(form_to_save)),
      client_(client),
      saving_flow_recorder_(
          std::make_unique<password_manager::SavingFlowMetricsRecorder>()) {}

OnboardingDialogView::~OnboardingDialogView() {
  Java_OnboardingDialogBridge_destroy(base::android::AttachCurrentThread(),
                                      java_object_);
}

void OnboardingDialogView::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::WindowAndroid* window_android =
      client_->web_contents()->GetTopLevelNativeWindow();
  java_object_.Reset(Java_OnboardingDialogBridge_create(
      env, window_android->GetJavaObject(), reinterpret_cast<intptr_t>(this)));

  base::string16 onboarding_title, onboarding_details;
  std::tie(onboarding_title, onboarding_details) =
      GetOnboardingTitleAndDetails();

  Java_OnboardingDialogBridge_showDialog(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, onboarding_title),
      base::android::ConvertUTF16ToJavaString(env, onboarding_details));

  client_->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordManagerOnboardingState,
      static_cast<int>(
          password_manager::metrics_util::OnboardingState::kShown));

  saving_flow_recorder_->SetOnboardingShown();
}

void OnboardingDialogView::DismissWithReasonAndDelete(
    password_manager::metrics_util::OnboardingUIDismissalReason reason) {
  password_manager::metrics_util::LogOnboardingUIDismissalReason(reason);
  if (saving_flow_recorder_) {
    saving_flow_recorder_->SetFlowResult(reason);
  }
  delete this;
}

void OnboardingDialogView::OnboardingAccepted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  client_->OnOnboardingSuccessful(std::move(form_to_save_),
                                  std::move(saving_flow_recorder_));
  DismissWithReasonAndDelete(
      password_manager::metrics_util::OnboardingUIDismissalReason::kAccepted);
}

void OnboardingDialogView::OnboardingRejected(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DismissWithReasonAndDelete(
      password_manager::metrics_util::OnboardingUIDismissalReason::kRejected);
}

void OnboardingDialogView::OnboardingAborted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DismissWithReasonAndDelete(
      password_manager::metrics_util::OnboardingUIDismissalReason::kDismissed);
}
