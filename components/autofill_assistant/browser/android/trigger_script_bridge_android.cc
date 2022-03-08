// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/trigger_script_bridge_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill_assistant/android/jni_headers/AssistantTriggerScriptBridge_jni.h"
#include "components/autofill_assistant/browser/android/assistant_header_model.h"
#include "components/autofill_assistant/browser/android/dependencies.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;

namespace autofill_assistant {

TriggerScriptBridgeAndroid::TriggerScriptBridgeAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents,
    const base::android::JavaRef<jobject>& jassistant_deps)
    : dependencies_(Dependencies::CreateFromJavaDependencies(jassistant_deps)) {
  java_object_ = Java_AssistantTriggerScriptBridge_Constructor(
      env, jweb_contents, jassistant_deps);
  Java_AssistantTriggerScriptBridge_setNativePtr(
      AttachCurrentThread(), java_object_, reinterpret_cast<intptr_t>(this));
}

TriggerScriptBridgeAndroid::~TriggerScriptBridgeAndroid() {
  Detach();
}

void TriggerScriptBridgeAndroid::Attach(
    TriggerScriptCoordinator* trigger_script_coordinator) {
  trigger_script_coordinator_ = trigger_script_coordinator;
}

void TriggerScriptBridgeAndroid::Detach() {
  if (java_object_) {
    Java_AssistantTriggerScriptBridge_clearNativePtr(AttachCurrentThread(),
                                                     java_object_);
    java_object_ = nullptr;
  }
  trigger_script_coordinator_ = nullptr;
}

void TriggerScriptBridgeAndroid::OnTriggerScriptAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint action) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->PerformTriggerScriptAction(
      static_cast<TriggerScriptProto::TriggerScriptAction>(action));
}

void TriggerScriptBridgeAndroid::OnBottomSheetClosedWithSwipe(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->OnBottomSheetClosedWithSwipe();
}

bool TriggerScriptBridgeAndroid::OnBackButtonPressed(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  if (!trigger_script_coordinator_) {
    return false;
  }
  return trigger_script_coordinator_->OnBackButtonPressed();
}

void TriggerScriptBridgeAndroid::OnKeyboardVisibilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jvisible) {
  if (!trigger_script_coordinator_) {
    return;
  }
  trigger_script_coordinator_->OnKeyboardVisibilityChanged(jvisible);
}

void TriggerScriptBridgeAndroid::ShowTriggerScript(
    const TriggerScriptUIProto& proto) {
  if (!java_object_) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  auto jheader_model =
      Java_AssistantTriggerScriptBridge_createHeaderAndGetModel(env,
                                                                java_object_);
  AssistantHeaderModel header_model(jheader_model);
  header_model.SetStatusMessage(proto.status_message());
  header_model.SetBubbleMessage(proto.callout_message());
  header_model.SetProgressVisible(proto.has_progress_bar());
  if (proto.has_progress_bar()) {
    ShowProgressBarProto::StepProgressBarConfiguration configuration;
    for (const auto& icon : proto.progress_bar().step_icons()) {
      *configuration.add_annotated_step_icons()->mutable_icon() = icon;
    }
    auto jcontext =
        Java_AssistantTriggerScriptBridge_getContext(env, java_object_);
    header_model.SetStepProgressBarConfiguration(configuration, jcontext,
                                                 *dependencies_);
    header_model.SetProgressActiveStep(proto.progress_bar().active_step());
  }

  std::vector<ChipProto> left_aligned_chips;
  std::vector<int> left_aligned_chip_actions;
  for (const auto& chip : proto.left_aligned_chips()) {
    left_aligned_chips.emplace_back(chip.chip());
    left_aligned_chip_actions.emplace_back(static_cast<int>(chip.action()));
  }
  auto jleft_aligned_chips =
      ui_controller_android_utils::CreateJavaAssistantChipList(
          env, left_aligned_chips);

  std::vector<ChipProto> right_aligned_chips;
  std::vector<int> right_aligned_chip_actions;
  for (const auto& chip : proto.right_aligned_chips()) {
    right_aligned_chips.emplace_back(chip.chip());
    right_aligned_chip_actions.emplace_back(static_cast<int>(chip.action()));
  }
  auto jright_aligned_chips =
      ui_controller_android_utils::CreateJavaAssistantChipList(
          env, right_aligned_chips);

  std::vector<std::string> cancel_popup_items;
  std::vector<int> cancel_popup_actions;
  for (const auto& choice : proto.cancel_popup().choices()) {
    cancel_popup_items.emplace_back(choice.text());
    cancel_popup_actions.emplace_back(static_cast<int>(choice.action()));
  }

  jboolean success = Java_AssistantTriggerScriptBridge_showTriggerScript(
      env, java_object_, ToJavaArrayOfStrings(env, cancel_popup_items),
      ToJavaIntArray(env, cancel_popup_actions), jleft_aligned_chips,
      ToJavaIntArray(env, left_aligned_chip_actions), jright_aligned_chips,
      ToJavaIntArray(env, right_aligned_chip_actions),
      proto.resize_visual_viewport(), proto.scroll_to_hide());
  trigger_script_coordinator_->OnTriggerScriptShown(success);
}

void TriggerScriptBridgeAndroid::HideTriggerScript() {
  if (!java_object_) {
    return;
  }
  Java_AssistantTriggerScriptBridge_hideTriggerScript(AttachCurrentThread(),
                                                      java_object_);
}

}  // namespace autofill_assistant
