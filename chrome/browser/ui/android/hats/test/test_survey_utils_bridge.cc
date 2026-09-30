// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/test/test_survey_utils_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/hats/test/jni_headers/TestSurveyUtilsBridge_jni.h"

namespace hats {

// static
void TestSurveyUtilsBridge::SetUpJavaTestSurveyFactory() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestSurveyUtilsBridge_setupTestSurveyFactory(env);
}

// static
void TestSurveyUtilsBridge::ResetJavaTestSurveyFactory() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestSurveyUtilsBridge_reset(env);
}

// static
std::string TestSurveyUtilsBridge::GetLastShownSurveyTriggerId() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TestSurveyUtilsBridge_getLastShownTriggerId(env);
}

}  // namespace hats

DEFINE_JNI(TestSurveyUtilsBridge)
