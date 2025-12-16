// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_prompt_view_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/autofill/android/autofill_ai_save_update_entity_prompt_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillAiSaveUpdateEntityPromptViewAndroid::
    AutofillAiSaveUpdateEntityPromptViewAndroid(
        content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

AutofillAiSaveUpdateEntityPromptViewAndroid::
    ~AutofillAiSaveUpdateEntityPromptViewAndroid() {
  // TODO: crbug.com/460410690 - Reset java object.
}

bool AutofillAiSaveUpdateEntityPromptViewAndroid::Show(
    const AutofillAiSaveUpdateEntityPromptController* controller) {
  CHECK(controller);
  if (!web_contents_->GetTopLevelNativeWindow()) {
    return false;  // No window attached (yet or anymore).
  }

  SetContent(controller);
  // TODO: crbug.com/460410690 - Show prompt.
  return true;
}

void AutofillAiSaveUpdateEntityPromptViewAndroid::SetContent(
    const AutofillAiSaveUpdateEntityPromptController* controller) {
  CHECK(controller);
  CHECK(java_object_);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> title =
      base::android::ConvertUTF16ToJavaString(env, controller->GetTitle());
  ScopedJavaLocalRef<jstring> positive_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetPositiveButtonText());
  ScopedJavaLocalRef<jstring> negative_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, controller->GetNegativeButtonText());

  // TODO: crbug.com/460410690 - Set dialog details.
}

}  // namespace autofill
