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
#include "chrome/browser/autofill/android/entity_attribute_update_details_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/AutofillAiSaveUpdateEntityPrompt_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillAiSaveUpdateEntityPromptViewAndroid::
    AutofillAiSaveUpdateEntityPromptViewAndroid(
        content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

AutofillAiSaveUpdateEntityPromptViewAndroid::
    ~AutofillAiSaveUpdateEntityPromptViewAndroid() {
  if (java_object_) {
    Java_AutofillAiSaveUpdateEntityPrompt_dismiss(
        base::android::AttachCurrentThread(), java_object_);
    java_object_.Reset();
  }
}

bool AutofillAiSaveUpdateEntityPromptViewAndroid::Show(
    const AutofillAiSaveUpdateEntityPromptController* controller) {
  CHECK(controller);
  if (!web_contents_->GetTopLevelNativeWindow()) {
    return false;  // No window attached (yet or anymore).
  }

  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller->GetJavaObject();
  if (!java_controller) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_AutofillAiSaveUpdateEntityPrompt_create(
      env, web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
      java_controller));
  if (!java_object_) {
    return false;
  }

  SetContent(controller);
  Java_AutofillAiSaveUpdateEntityPrompt_show(env, java_object_);
  return true;
}

void AutofillAiSaveUpdateEntityPromptViewAndroid::SetContent(
    const AutofillAiSaveUpdateEntityPromptController* controller) {
  CHECK(controller);
  CHECK(java_object_);

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_AutofillAiSaveUpdateEntityPrompt_setDialogDetails(
      env, java_object_, controller->GetTitle(),
      controller->GetPositiveButtonText(), controller->GetNegativeButtonText(),
      controller->IsWalletableEntity());

  Java_AutofillAiSaveUpdateEntityPrompt_setEntityUpdateDetails(
      env, java_object_, controller->GetEntityUpdateDetails(),
      controller->IsUpdatePrompt());

  Java_AutofillAiSaveUpdateEntityPrompt_setSourceNotice(
      env, java_object_, controller->GetSourceNotice(),
      controller->IsWalletableEntity());
}

}  // namespace autofill

DEFINE_JNI(AutofillAiSaveUpdateEntityPrompt)
