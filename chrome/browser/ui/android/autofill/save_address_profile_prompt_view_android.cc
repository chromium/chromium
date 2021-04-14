// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/save_address_profile_prompt_view_android.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SaveAddressProfilePrompt_jni.h"
#include "chrome/browser/autofill/android/save_address_profile_prompt_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

SaveAddressProfilePromptViewAndroid::SaveAddressProfilePromptViewAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

SaveAddressProfilePromptViewAndroid::~SaveAddressProfilePromptViewAndroid() {
  if (java_object_) {
    Java_SaveAddressProfilePrompt_dismiss(base::android::AttachCurrentThread(),
                                          java_object_);
  }
}

bool SaveAddressProfilePromptViewAndroid::Show(
    SaveAddressProfilePromptController* controller) {
  DCHECK(controller);
  if (!web_contents_->GetTopLevelNativeWindow()) {
    return false;  // No window attached (yet or anymore).
  }

  base::android::ScopedJavaLocalRef<jobject> java_controller =
      controller->GetJavaObject();
  if (!java_controller)
    return false;

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> address =
      base::android::ConvertUTF16ToJavaString(env, controller->GetAddress());
  ScopedJavaLocalRef<jstring> email =
      base::android::ConvertUTF16ToJavaString(env, controller->GetEmail());
  ScopedJavaLocalRef<jstring> phone = base::android::ConvertUTF16ToJavaString(
      env, controller->GetPhoneNumber());
  java_object_.Reset(Java_SaveAddressProfilePrompt_show(
      env, web_contents_->GetTopLevelNativeWindow()->GetJavaObject(),
      java_controller, address, email, phone));
  return !!java_object_;
}

}  // namespace autofill
