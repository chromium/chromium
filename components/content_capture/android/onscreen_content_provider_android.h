// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_ANDROID_ONSCREEN_CONTENT_PROVIDER_ANDROID_H_
#define COMPONENTS_CONTENT_CAPTURE_ANDROID_ONSCREEN_CONTENT_PROVIDER_ANDROID_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "components/content_capture/browser/content_capture_consumer.h"
#include "components/content_capture/browser/onscreen_content_provider.h"

namespace content_capture {

// The Android's implementation of OnscreenContentProvider, it forwards
// the received message to Java and switches among the OnscreenContentProvider.
class OnscreenContentProviderAndroid : public ContentCaptureConsumer {
 public:
  OnscreenContentProviderAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobject,
      content::WebContents* web_contents);
  ~OnscreenContentProviderAndroid() override;

  // ContentCaptureConsumer
  void DidCaptureContent(const ContentCaptureSession& parent_session,
                         const ContentCaptureFrame& data) override;
  void DidUpdateContent(const ContentCaptureSession& parent_session,
                        const ContentCaptureFrame& data) override;
  void DidRemoveContent(const ContentCaptureSession& session,
                        const std::vector<int64_t>& data) override;
  void DidRemoveSession(const ContentCaptureSession& session) override;
  void DidUpdateTitle(const ContentCaptureFrame& main_frame) override;
  void DidUpdateFavicon(const ContentCaptureFrame& main_frame) override;
  bool ShouldCapture(const GURL& url) override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void OnWebContentsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);
  void Destroy(JNIEnv* env);

 private:
  void AttachToWebContents(content::WebContents* web_contennts);
  void DetachFromWebContents();
  content::WebContents* GetWebContents();

  base::WeakPtr<OnscreenContentProvider> onscreen_content_provider_;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_ANDROID_ONSCREEN_CONTENT_PROVIDER_ANDROID_H_
