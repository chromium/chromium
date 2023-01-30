// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THIN_WEBVIEW_INTERNAL_COMPOSITOR_VIEW_IMPL_H_
#define COMPONENTS_THIN_WEBVIEW_INTERNAL_COMPOSITOR_VIEW_IMPL_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/thin_webview/compositor_view.h"
#include "content/public/browser/android/compositor_client.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
}  // namespace cc::slim

namespace content {
class Compositor;
}  // namespace content

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace thin_webview {
namespace android {

// Native counterpart of CompositorViewImpl.java.
class CompositorViewImpl : public CompositorView,
                           public content::CompositorClient {
 public:
  CompositorViewImpl(JNIEnv* env,
                     jobject obj,
                     ui::WindowAndroid* window_android,
                     int64_t java_background_color);

  CompositorViewImpl(const CompositorViewImpl&) = delete;
  CompositorViewImpl& operator=(const CompositorViewImpl&) = delete;

  ~CompositorViewImpl() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& object);

  void SetNeedsComposite(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& object);
  void SurfaceCreated(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object);
  void SurfaceDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& object);
  void SurfaceChanged(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& object,
                      jint format,
                      jint width,
                      jint height,
                      bool can_be_used_with_surface_control,
                      const base::android::JavaParamRef<jobject>& surface);

  // CompositorView implementation.
  void SetRootLayer(scoped_refptr<cc::slim::Layer> layer) override;

  // CompositorClient implementation.
  void RecreateSurface() override;
  void UpdateLayerTreeHost() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> obj_;
  std::unique_ptr<content::Compositor> compositor_;
  scoped_refptr<cc::slim::SolidColorLayer> root_layer_;

  int current_surface_format_;
};

}  // namespace android
}  // namespace thin_webview

#endif  // COMPONENTS_THIN_WEBVIEW_INTERNAL_COMPOSITOR_VIEW_IMPL_H_
