// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillAiSaveUpdateEntityPromptController;

// JNI wrapper for Java AutofillAiSaveUpdateEntityPrompt.
class AutofillAiSaveUpdateEntityPromptViewAndroid
    : public AutofillAiSaveUpdateEntityPromptView {
 public:
  explicit AutofillAiSaveUpdateEntityPromptViewAndroid(
      content::WebContents* web_contents);
  AutofillAiSaveUpdateEntityPromptViewAndroid(
      const AutofillAiSaveUpdateEntityPromptViewAndroid&) = delete;
  AutofillAiSaveUpdateEntityPromptViewAndroid& operator=(
      const AutofillAiSaveUpdateEntityPromptViewAndroid&) = delete;
  ~AutofillAiSaveUpdateEntityPromptViewAndroid() override;

  // AutofillAiSaveUpdateEntityPromptViewAndroid:
  bool Show(
      const AutofillAiSaveUpdateEntityPromptController* controller) override;

 private:
  void SetContent(const AutofillAiSaveUpdateEntityPromptController* controller);

  // The corresponding Java AutofillAiSaveUpdateEntityPrompt owned by this
  // class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  raw_ref<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_SAVE_UPDATE_ENTITY_PROMPT_VIEW_ANDROID_H_
