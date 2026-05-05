// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"

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

  // Captures a screenshot of the current window and initiates the overlay UI.
  // If the asynchronous capture fails, the process silently aborts. Returns
  // true if the capture process was successfully started, or false otherwise
  // (e.g., if the window is unavailable).
  bool ShowUI(JNIEnv* env, int32_t invocation_source);
  void Destroy(JNIEnv* env);

 private:
  void OnScreenshotCaptured(gfx::Image snapshot);

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<LensOverlayControllerAndroid> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_
