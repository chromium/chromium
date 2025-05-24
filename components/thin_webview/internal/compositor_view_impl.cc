// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/thin_webview/internal/compositor_view_impl.h"

#include <memory>

#include "base/android/jni_android.h"
#include "cc/slim/layer.h"
#include "cc/slim/solid_color_layer.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/color_utils_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/thin_webview/internal/jni_headers/CompositorViewImpl_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace thin_webview {
namespace android {
namespace {
const int kPixelFormatUnknown = 0;
}  // namespace

jlong JNI_CompositorViewImpl_Init(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jobject>& jwindow_android,
                                  jint java_background_color) {
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  auto compositor_view = std::make_unique<CompositorViewImpl>(
      env, obj, window_android, java_background_color);
  return reinterpret_cast<intptr_t>(compositor_view.release());
}

// static
CompositorView* CompositorView::FromJavaObject(
    const base::android::JavaRef<jobject>& jcompositor_view) {
  if (jcompositor_view.is_null())
    return nullptr;

  return reinterpret_cast<CompositorViewImpl*>(
      Java_CompositorViewImpl_getNativePtr(base::android::AttachCurrentThread(),
                                           jcompositor_view));
}

CompositorViewImpl::CompositorViewImpl(JNIEnv* env,
                                       jobject obj,
                                       ui::WindowAndroid* window_android,
                                       int64_t java_background_color)
    : obj_(env, obj),
      root_layer_(cc::slim::SolidColorLayer::Create()),
      current_surface_format_(kPixelFormatUnknown) {
  compositor_.reset(content::Compositor::Create(this, window_android));
  root_layer_->SetIsDrawable(true);
  std::optional<SkColor> background_color =
      ui::JavaColorToOptionalSkColor(java_background_color);
  // TODO(crbug.com/40219248): Remove FromColor and make all SkColor4f.
  root_layer_->SetBackgroundColor(
      SkColor4f::FromColor(background_color.value()));
}

CompositorViewImpl::~CompositorViewImpl() = default;

void CompositorViewImpl::Destroy(JNIEnv* env,
                                 const JavaParamRef<jobject>& object) {
  delete this;
}

void CompositorViewImpl::SurfaceCreated(JNIEnv* env,
                                        const JavaParamRef<jobject>& object) {
  compositor_->SetRootLayer(root_layer_);
  current_surface_format_ = kPixelFormatUnknown;
}

void CompositorViewImpl::SurfaceDestroyed(JNIEnv* env,
                                          const JavaParamRef<jobject>& object) {
  // When we switch from Chrome to other app we can't detach child surface
  // controls because it leads to a visible hole: b/157439199. To avoid this we
  // don't detach surfaces if the surface is going to be destroyed, they will be
  // detached and freed by OS.
  compositor_->PreserveChildSurfaceControls();

  compositor_->SetSurface(nullptr, false, nullptr);
  current_surface_format_ = kPixelFormatUnknown;
}

void CompositorViewImpl::SurfaceChanged(JNIEnv* env,
                                        const JavaParamRef<jobject>& object,
                                        jint format,
                                        jint width,
                                        jint height,
                                        bool can_be_used_with_surface_control,
                                        const JavaParamRef<jobject>& surface) {
  DCHECK(surface);
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    compositor_->SetSurface(surface, can_be_used_with_surface_control, nullptr);
  }

  gfx::Size content_size(width, height);
  compositor_->SetWindowBounds(content_size);
  root_layer_->SetBounds(content_size);
}

void CompositorViewImpl::SetNeedsComposite(
    JNIEnv* env,
    const JavaParamRef<jobject>& object) {
  compositor_->SetNeedsComposite();
}

void CompositorViewImpl::SetRootLayer(scoped_refptr<cc::slim::Layer> layer) {
  const auto& children = root_layer_->children();
  DCHECK(children.size() <= 1);
  if (!children.empty() && children[0]->id() == layer->id())
    return;

  root_layer_->RemoveAllChildren();
  root_layer_->InsertChild(layer, 0);
}

void CompositorViewImpl::RecreateSurface() {
  JNIEnv* env = base::android::AttachCurrentThread();
  compositor_->SetSurface(nullptr, false, nullptr);
  Java_CompositorViewImpl_recreateSurface(env, obj_);
}

void CompositorViewImpl::UpdateLayerTreeHost() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CompositorViewImpl_onCompositorLayout(env, obj_);
}

}  // namespace android
}  // namespace thin_webview
