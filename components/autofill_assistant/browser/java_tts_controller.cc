// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/java_tts_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/test_support_jni_headers/AutofillAssistantTestTtsController_jni.h"

namespace autofill_assistant {

static jlong JNI_AutofillAssistantTestTtsController_CreateNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_tts_controller) {
  return reinterpret_cast<jlong>(new TtsControllerAndroid(java_tts_controller));
}

TtsControllerAndroid::TtsControllerAndroid(
    const base::android::JavaParamRef<jobject>& java_tts_controller)
    : AutofillAssistantTtsController(nullptr),
      java_tts_controller_(java_tts_controller) {}
TtsControllerAndroid::~TtsControllerAndroid() = default;

void TtsControllerAndroid::Speak(const std::string& message,
                                 const std::string& locale) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAssistantTestTtsController_speak(
      env, java_tts_controller_,
      base::android::ConvertUTF8ToJavaString(env, message),
      base::android::ConvertUTF8ToJavaString(env, locale));
}

void TtsControllerAndroid::Stop() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAssistantTestTtsController_stop(env, java_tts_controller_);
}

void TtsControllerAndroid::OnTtsEvent(content::TtsUtterance* utterance,
                                      content::TtsEventType event_type,
                                      int char_index,
                                      int char_length,
                                      const std::string& error_message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAssistantTestTtsController_onTtsEvent(
      env, java_tts_controller_, static_cast<int>(event_type));
}

}  // namespace autofill_assistant
