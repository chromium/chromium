// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/wallet/android/boarding_pass_detector.h"
#include "content/public/browser/web_contents.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/wallet/android/jni_headers/BoardingPassBridge_jni.h"

using jni_zero::JavaRef;

namespace wallet {

namespace {
base::OnceCallback<void(const std::vector<std::string>&)> AdaptCallbackForJava(
    const JavaRef<jobject>& jcallback) {
  auto adaptor = [](const JavaRef<jobject>& jcallback,
                    const std::vector<std::string>& result) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    base::android::RunObjectCallbackAndroid(
        jcallback, base::android::ToJavaArrayOfStrings(env, std::move(result)));
  };

  return base::BindOnce(adaptor,
                        jni_zero::ScopedJavaGlobalRef<jobject>(jcallback));
}
}  // namespace

static bool JNI_BoardingPassBridge_ShouldDetect(const std::string& url) {
  return BoardingPassDetector::ShouldDetect(url);
}

static void JNI_BoardingPassBridge_DetectBoardingPass(
    content::WebContents* web_contents,
    const JavaRef<jobject>& jcallback) {
  // BoardingPassDetector is auto deleting.
  BoardingPassDetector* detector = new BoardingPassDetector();
  auto callback = AdaptCallbackForJava(jcallback);
  detector->DetectBoardingPass(web_contents, std::move(callback));
}

}  // namespace wallet

DEFINE_JNI(BoardingPassBridge)
