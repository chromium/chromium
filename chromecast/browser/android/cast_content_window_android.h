// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_
#define CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chromecast/browser/cast_content_window.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

// Android implementation of CastContentWindow, which displays WebContents in
// CastWebContentsActivity.
class CastContentWindowAndroid : public CastContentWindow {
 public:
  explicit CastContentWindowAndroid(
      const CastContentWindow::CreateParams& params);
  ~CastContentWindowAndroid() override;

  // CastContentWindow implementation:
  void CreateWindowForWebContents(
      CastWebContents* cast_web_contents,
      mojom::ZOrder z_order,
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
  bool ConsumeGesture(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller,
                      int gesture_type);
  void OnVisibilityChange(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jcaller,
                          int visibility_type);
  base::android::ScopedJavaLocalRef<jstring> GetId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  const std::string activity_id_;
  base::android::ScopedJavaGlobalRef<jobject> java_window_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindowAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_CAST_CONTENT_WINDOW_ANDROID_H_
