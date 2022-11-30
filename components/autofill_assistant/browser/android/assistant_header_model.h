// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_HEADER_MODEL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_HEADER_MODEL_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/android/assistant_header_delegate.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/tts_button_state.h"

namespace autofill_assistant {

// C++ equivalent to java-side |AssistantHeaderModel|.
class AssistantHeaderModel {
 public:
  AssistantHeaderModel(
      const base::android::ScopedJavaLocalRef<jobject>& jmodel);
  ~AssistantHeaderModel();

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject() const;

  void SetDelegate(const AssistantHeaderDelegate& delegate);
  void SetStatusMessage(const std::string& status_message);
  void SetBubbleMessage(const std::string& bubble_message);
  void SetProfileIconMenuSettingsMessage(
      const std::string& profile_icon_menu_settings_message);
  void SetProfileIconMenuSendFeedbackMessage(
      const std::string& profile_icon_menu_send_feedback_message);
  void SetProgressActiveStep(int active_step);
  void SetProgressVisible(bool visible);
  void SetProgressBarErrorState(bool error);
  void SetStepProgressBarConfiguration(
      const ShowProgressBarProto::StepProgressBarConfiguration& configuration,
      const base::android::JavaRef<jobject>& jcontext,
      const DependenciesAndroid& dependencies);
  void SetSpinPoodle(bool enabled);
  void SetChips(const base::android::ScopedJavaLocalRef<jobject>& jchips);
  void SetTtsButtonVisible(bool visible);
  void SetTtsButtonState(TtsButtonState state);
  void SetDisableAnimations(bool disable_animations);

 private:
  // Java-side AssistantHeaderModel object.
  base::android::ScopedJavaGlobalRef<jobject> jmodel_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_HEADER_MODEL_H_
