// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/survey_ui_delegate_android.h"

#include "components/messages/android/message_wrapper.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/hats/internal/jni_headers/SurveyUiDelegateBridge_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace hats {

SurveyUiDelegateAndroid::SurveyUiDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobj_ =
      Java_SurveyUiDelegateBridge_create(env, reinterpret_cast<int64_t>(this));
}

SurveyUiDelegateAndroid::SurveyUiDelegateAndroid(
    messages::MessageWrapper* messages,
    ui::WindowAndroid* window) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobj_ = Java_SurveyUiDelegateBridge_createFromMessage(
      env, reinterpret_cast<int64_t>(this), messages->GetJavaMessageWrapper(),
      window->GetJavaObject());
}

SurveyUiDelegateAndroid::~SurveyUiDelegateAndroid() = default;

void SurveyUiDelegateAndroid::ShowSurveyInvitation(
    JNIEnv* env,
    const JavaParamRef<jobject>& on_accepted_callback,
    const JavaParamRef<jobject>& on_declined_callback,
    const JavaParamRef<jobject>& on_presentation_failed_callback) {
  LOG(WARNING) << "Unimplemented SurveyUiDelegateAndroid::ShowSurveyInvitation";
}

void SurveyUiDelegateAndroid::Dismiss(JNIEnv* env) {
  LOG(WARNING) << "Unimplemented SurveyUiDelegateAndroid::Dismiss";
}

const JavaRef<jobject>& SurveyUiDelegateAndroid::GetJavaObject(
    JNIEnv* env) const {
  return jobj_;
}

}  // namespace hats
