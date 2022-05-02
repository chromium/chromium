// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_header_model.h"

#include "base/android/jni_string.h"
#include "components/autofill_assistant/android/jni_headers/AssistantHeaderModel_jni.h"
#include "components/autofill_assistant/browser/android/ui_controller_android.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"

using base::android::AttachCurrentThread;

namespace autofill_assistant {

AssistantHeaderModel::AssistantHeaderModel(
    const base::android::ScopedJavaLocalRef<jobject>& jmodel)
    : jmodel_(jmodel) {}

AssistantHeaderModel::~AssistantHeaderModel() = default;

void AssistantHeaderModel::SetDelegate(
    const AssistantHeaderDelegate& delegate) {
  Java_AssistantHeaderModel_setDelegate(AttachCurrentThread(), jmodel_,
                                        delegate.GetJavaObject());
}

void AssistantHeaderModel::SetStatusMessage(const std::string& status_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantHeaderModel_setStatusMessage(
      env, jmodel_,
      base::android::ConvertUTF8ToJavaString(env, status_message));
}

void AssistantHeaderModel::SetBubbleMessage(const std::string& bubble_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantHeaderModel_setBubbleMessage(
      env, jmodel_,
      base::android::ConvertUTF8ToJavaString(env, bubble_message));
}

void AssistantHeaderModel::SetProfileIconMenuSettingsMessage(
    const std::string& profile_icon_menu_settings_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantHeaderModel_setProfileIconMenuSettingsMessage(
      env, jmodel_,
      base::android::ConvertUTF8ToJavaString(
          env, profile_icon_menu_settings_message));
}

void AssistantHeaderModel::SetProfileIconMenuSendFeedbackMessage(
    const std::string& profile_icon_menu_send_feedback_message) {
  JNIEnv* env = AttachCurrentThread();
  Java_AssistantHeaderModel_setProfileIconMenuSendFeedbackMessage(
      env, jmodel_,
      base::android::ConvertUTF8ToJavaString(
          env, profile_icon_menu_send_feedback_message));
}

void AssistantHeaderModel::SetProgressActiveStep(int active_step) {
  Java_AssistantHeaderModel_setProgressActiveStep(AttachCurrentThread(),
                                                  jmodel_, active_step);
}

void AssistantHeaderModel::SetProgressVisible(bool visible) {
  Java_AssistantHeaderModel_setProgressVisible(AttachCurrentThread(), jmodel_,
                                               visible);
}

void AssistantHeaderModel::SetProgressBarErrorState(bool error) {
  Java_AssistantHeaderModel_setProgressBarErrorState(AttachCurrentThread(),
                                                     jmodel_, error);
}

void AssistantHeaderModel::SetStepProgressBarConfiguration(
    const ShowProgressBarProto::StepProgressBarConfiguration& configuration,
    const base::android::JavaRef<jobject>& jcontext,
    const DependenciesAndroid& dependencies) {
  JNIEnv* env = AttachCurrentThread();
  if (!configuration.annotated_step_icons().empty()) {
    auto jlist = Java_AssistantHeaderModel_createIconList(env);
    for (const auto& icon : configuration.annotated_step_icons()) {
      Java_AssistantHeaderModel_addStepProgressBarIcon(
          env, jlist,
          ui_controller_android_utils::CreateJavaDrawable(
              env, jcontext, dependencies, icon.icon()));
    }
    Java_AssistantHeaderModel_setStepProgressBarIcons(env, jmodel_, jlist);
  }
}

void AssistantHeaderModel::SetSpinPoodle(bool enabled) {
  Java_AssistantHeaderModel_setSpinPoodle(AttachCurrentThread(), jmodel_,
                                          enabled);
}

void AssistantHeaderModel::SetChips(
    const base::android::ScopedJavaLocalRef<jobject>& jchips) {
  Java_AssistantHeaderModel_setChips(AttachCurrentThread(), jmodel_, jchips);
}

void AssistantHeaderModel::SetTtsButtonVisible(bool visible) {
  Java_AssistantHeaderModel_setTtsButtonVisible(AttachCurrentThread(), jmodel_,
                                                visible);
}

void AssistantHeaderModel::SetTtsButtonState(TtsButtonState state) {
  Java_AssistantHeaderModel_setTtsButtonState(AttachCurrentThread(), jmodel_,
                                              state);
}

void AssistantHeaderModel::SetDisableAnimations(bool disable_animations) {
  Java_AssistantHeaderModel_setDisableAnimations(AttachCurrentThread(), jmodel_,
                                                 disable_animations);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantHeaderModel::GetJavaObject() const {
  return jmodel_;
}

}  // namespace autofill_assistant
