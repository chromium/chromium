// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_
#define CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chromecast/browser/cast_content_window.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

// Android implementation of CastContentWindow, which displays WebContents in
// CastWebContentsActivity.
class CastContentWindowAndroid : public CastContentWindow {
 public:
  explicit CastContentWindowAndroid(mojom::CastWebViewParamsPtr params);

  CastContentWindowAndroid(const CastContentWindowAndroid&) = delete;
  CastContentWindowAndroid& operator=(const CastContentWindowAndroid&) = delete;

  ~CastContentWindowAndroid() override;

  // CastContentWindow implementation:
  void CreateWindow(mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;
  void EnableTouchInput(bool enabled) override;
  void RequestVisibility(VisibilityPriority visibility_priority) override;
  void SetActivityContext(base::Value activity_context) override;
  void SetHostContext(base::Value host_context) override;
  void NotifyVisibilityChange(VisibilityType visibility_type) override;
  void RequestMoveOut() override;

  // Called through JNI.
  void OnActivityStopped(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller);
  void ConsumeGesture(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      int gesture_type,
      const base::android::JavaParamRef<jobject>& handled_callback);
  void OnVisibilityChange(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller,
                          int visibility_type);

 private:
  bool web_contents_attached_;
  base::android::ScopedJavaGlobalRef<jobject> java_window_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_
