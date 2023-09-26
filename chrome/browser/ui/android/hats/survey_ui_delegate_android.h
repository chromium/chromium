// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_UI_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_UI_DELEGATE_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace messages {
class MessageWrapper;
}  // namespace messages

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace hats {

class SurveyClientAndroid;

// Interface used to display Survey to the Android UI. C++ clients can extend
// this class, so the C++ instance will receive the corresponding method calls
// from Java.
// To use the default Java Clank message implementation in C++, create
// an instance through |createFromMessage|.
class SurveyUiDelegateAndroid {
 public:
  // Create an empty SurveyUiDelegateAndroid instance that's associated with
  // Java.
  SurveyUiDelegateAndroid();

  // Create an instance with Clank Message.
  SurveyUiDelegateAndroid(messages::MessageWrapper* messages,
                          ui::WindowAndroid* windowAndroid);

  virtual ~SurveyUiDelegateAndroid();

  SurveyUiDelegateAndroid(const SurveyUiDelegateAndroid&) = delete;
  SurveyUiDelegateAndroid& operator=(const SurveyUiDelegateAndroid&) = delete;

  // Show the survey invitation. The input callbacks are used to communicate
  // with survey component internally.
  // * Run |on_accepted_callback| when survey invitation is accepted, and survey
  // component will present the survey to the user;
  // * Run |on_declined_callback| when survey invitation is declined.
  // * Run |on_presentation_failed_callback| when survey invitation is failed to
  // present at all.
  virtual void ShowSurveyInvitation(
      JNIEnv* env,
      const JavaParamRef<jobject>& on_accepted_callback,
      const JavaParamRef<jobject>& on_declined_callback,
      const JavaParamRef<jobject>& on_presentation_failed_callback);

  // Dismiss the survey invitation.
  virtual void Dismiss(JNIEnv* env);

 private:
  friend class SurveyClientAndroid;
  const JavaRef<jobject>& GetJavaObject(JNIEnv* env) const;

  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

}  // namespace hats

#endif  // CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_UI_DELEGATE_ANDROID_H_
