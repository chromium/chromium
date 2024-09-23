// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/wallet/android/boarding_pass_detector.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/wallet/android/jni_headers/BoardingPassBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace wallet {

namespace {
base::OnceCallback<void(const std::vector<std::string>&)> AdaptCallbackForJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcallback) {
  auto adaptor = [](const base::android::JavaRef<jobject>& jcallback,
                    const std::vector<std::string>& result) {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(
        jcallback, base::android::ToJavaArrayOfStrings(env, std::move(result)));
  };

  return base::BindOnce(adaptor,
                        base::android::ScopedJavaGlobalRef<jobject>(jcallback));
}
}  // namespace

static jboolean JNI_BoardingPassBridge_ShouldDetect(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl) {
  return BoardingPassDetector::ShouldDetect(ConvertJavaStringToUTF8(env, jurl));
}

static void JNI_BoardingPassBridge_DetectBoardingPass(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  // BoardignPassDetector is auto deleting.
  BoardingPassDetector* detector = new BoardingPassDetector();
  auto callback = AdaptCallbackForJava(env, jcallback);
  detector->DetectBoardingPass(web_contents, std::move(callback));
}

}  // namespace wallet
