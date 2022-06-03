// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JAVA_TTS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JAVA_TTS_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/autofill_assistant_tts_controller.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Thin C++ wrapper around a TTS controller implemented in Java.
// Intended for use in integration tests to inject as a mock TTS controller.
class TtsControllerAndroid : public AutofillAssistantTtsController {
 public:
  TtsControllerAndroid(
      const base::android::JavaParamRef<jobject>& jtts_controller);
  ~TtsControllerAndroid() override;
  TtsControllerAndroid(const TtsControllerAndroid&) = delete;
  TtsControllerAndroid& operator=(const TtsControllerAndroid&) = delete;

  // Overrides AutofillAssistantTtsController.
  void Speak(const std::string& message, const std::string& locale) override;
  void Stop() override;

  // Mimics receiving a content::TtsEventType for the current utterance.
  // |eventType| should be an integer representing a valid content::TtsEventType
  // value.
  void SimulateTtsEvent(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcaller,
                        jint eventType);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_tts_controller_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_JAVA_TTS_CONTROLLER_H_
