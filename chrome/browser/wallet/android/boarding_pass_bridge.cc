// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/wallet/android/boarding_pass_detector.h"
#include "chrome/browser/wallet/android/jni_headers/BoardingPassBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace wallet {

static jboolean JNI_BoardingPassBridge_ShouldDetect(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl) {
  return BoardingPassDetector::ShouldDetect(ConvertJavaStringToUTF8(env, jurl));
}

static void JNI_BoardingPassBridge_DetectBoardingPass(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  // TODO(crbug/1502402): Implement boarding pass bridge;
}

}  // namespace wallet
