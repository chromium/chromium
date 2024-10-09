// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_VIEW_CONTENT_VIEW_RENDER_VIEW_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_VIEW_CONTENT_VIEW_RENDER_VIEW_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "content/public/browser/android/compositor_client.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class Compositor;
}  // namespace content

namespace embedder_support {

class ContentViewRenderView : public content::CompositorClient {
 public:
  ContentViewRenderView(JNIEnv* env,
                        jobject obj,
                        gfx::NativeWindow root_window);

  ContentViewRenderView(const ContentViewRenderView&) = delete;
  ContentViewRenderView& operator=(const ContentViewRenderView&) = delete;

  // Methods called from Java via JNI -----------------------------------------
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetCurrentWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  void SurfaceCreated(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void SurfaceDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void SurfaceChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint format,
      jint width,
      jint height,
      const base::android::JavaParamRef<jobject>& surface,
      const base::android::JavaParamRef<jobject>& browser_input_token);
  void SetOverlayVideoMode(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool enabled);

  // CompositorClient implementation
  void UpdateLayerTreeHost() override;
  void DidSwapFrame(int pending_frames) override;

 private:
  ~ContentViewRenderView() override;

  void InitCompositor();

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  std::unique_ptr<content::Compositor> compositor_;

  gfx::NativeWindow root_window_;
  int current_surface_format_;
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_VIEW_CONTENT_VIEW_RENDER_VIEW_H_
