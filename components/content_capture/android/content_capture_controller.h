// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_ANDROID_CONTENT_CAPTURE_CONTROLLER_H_
#define COMPONENTS_CONTENT_CAPTURE_ANDROID_CONTENT_CAPTURE_CONTROLLER_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace content_capture {

// This class has one instance per process and is called by
// ContentReceiverManager to check if the given url shall be captured.
class ContentCaptureController {
 public:
  static ContentCaptureController* Get();

  // Not call constructor directly, instead, uses Get().
  ContentCaptureController();

  // Returns if the given |url| shall be captured.
  bool ShouldCapture(const GURL& url);

  // The methods called by Java peer.
  void SetWhitelist(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jcaller,
                    const base::android::JavaParamRef<jobjectArray>& jwhitelist,
                    const base::android::JavaParamRef<jbooleanArray>& jtype);
  void SetJavaPeer(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& jcaller);

 private:
  virtual ~ContentCaptureController();
  JavaObjectWeakGlobalRef java_ref_;
  base::Optional<bool> has_whitelist_;
  std::vector<std::string> whitelist_;
  std::vector<std::unique_ptr<re2::RE2>> whitelist_regex_;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_ANDROID_CONTENT_CAPTURE_CONTROLLER_H_
