// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"

namespace lens {

class LensOverlayControllerAndroid {
 public:
  LensOverlayControllerAndroid(JNIEnv* env,
                               const base::android::JavaRef<jobject>& obj,
                               content::WebContents* web_contents);
  LensOverlayControllerAndroid(const LensOverlayControllerAndroid&) = delete;
  LensOverlayControllerAndroid& operator=(const LensOverlayControllerAndroid&) =
      delete;
  ~LensOverlayControllerAndroid();

  void ShowUI(JNIEnv* env, int32_t invocation_source);
  void Destroy(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_
