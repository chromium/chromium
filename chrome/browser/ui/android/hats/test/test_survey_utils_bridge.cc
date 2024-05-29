// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/test/test_survey_utils_bridge.h"
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
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
  base::android::ScopedJavaLocalRef<jstring> jtrigger_id =
      Java_TestSurveyUtilsBridge_getLastShownTriggerId(env);

  return base::android::ConvertJavaStringToUTF8(jtrigger_id);
}

}  // namespace hats
