// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/overlay/overlay_window_android.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/surface_layer.h"
#include "chrome/android/chrome_jni_headers/PictureInPictureActivity_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/thin_webview/compositor_view.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "ui/android/window_android_compositor.h"

// static
std::unique_ptr<content::OverlayWindow> content::OverlayWindow::Create(
    PictureInPictureWindowController* controller) {
  return std::make_unique<OverlayWindowAndroid>(controller);
}

OverlayWindowAndroid::OverlayWindowAndroid(
    content::PictureInPictureWindowController* controller)
    : window_android_(nullptr),
      compositor_view_(nullptr),
      surface_layer_(cc::SurfaceLayer::Create()),
      bounds_(gfx::Rect(0, 0)),
      controller_(controller) {
  surface_layer_->SetIsDrawable(true);
  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetMayContainVideo(true);
  surface_layer_->SetBackgroundColor(SK_ColorBLACK);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_createActivity(
      env, reinterpret_cast<intptr_t>(this),
      TabAndroid::FromWebContents(controller_->GetInitiatorWebContents())
          ->GetJavaObject());
}

OverlayWindowAndroid::~OverlayWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_onWindowDestroyed(
      env, reinterpret_cast<intptr_t>(this));
}

void OverlayWindowAndroid::OnActivityStart(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jwindow_android) {
  java_ref_ = JavaObjectWeakGlobalRef(env, obj);
  window_android_ = ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  window_android_->AddObserver(this);

  if (video_size_.IsEmpty())
    return;

  Java_PictureInPictureActivity_updateVideoSize(
      env, java_ref_.get(env), video_size_.width(), video_size_.height());
}

void OverlayWindowAndroid::OnAttachCompositor() {
  window_android_->GetCompositor()->AddChildFrameSink(
      surface_layer_->surface_id().frame_sink_id());
}

void OverlayWindowAndroid::OnDetachCompositor() {
  window_android_->GetCompositor()->RemoveChildFrameSink(
      surface_layer_->surface_id().frame_sink_id());
}

void OverlayWindowAndroid::OnActivityStopped() {
  Destroy(nullptr);
}

void OverlayWindowAndroid::Destroy(JNIEnv* env) {
  java_ref_.reset();

  if (window_android_) {
    window_android_->RemoveObserver(this);
    window_android_ = nullptr;
  }

  controller_->CloseAndFocusInitiator();
  controller_->OnWindowDestroyed();
}

void OverlayWindowAndroid::Play(JNIEnv* env) {
  DCHECK(!controller_->IsPlayerActive());
  controller_->TogglePlayPause();
}

void OverlayWindowAndroid::CompositorViewCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& compositor_view) {
  compositor_view_ =
      thin_webview::android::CompositorView::FromJavaObject(compositor_view);
  DCHECK(compositor_view_);
  compositor_view_->SetRootLayer(surface_layer_);
}

void OverlayWindowAndroid::OnViewSizeChanged(JNIEnv* env,
                                             jint width,
                                             jint height) {
  gfx::Size content_size(width, height);
  if (bounds_.size() == content_size)
    return;

  bounds_.set_size(content_size);
  surface_layer_->SetBounds(content_size);
  controller_->UpdateLayerBounds();
}

void OverlayWindowAndroid::Close() {
  if (java_ref_.is_uninitialized())
    return;

  DCHECK(window_android_);
  window_android_->RemoveObserver(this);
  window_android_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_close(env, java_ref_.get(env));
  controller_->OnWindowDestroyed();
}

void OverlayWindowAndroid::Hide() {
  Close();
}

bool OverlayWindowAndroid::IsActive() {
  return true;
}

bool OverlayWindowAndroid::IsVisible() {
  return true;
}

bool OverlayWindowAndroid::IsAlwaysOnTop() {
  return true;
}

gfx::Rect OverlayWindowAndroid::GetBounds() {
  return bounds_;
}

void OverlayWindowAndroid::UpdateVideoSize(const gfx::Size& natural_size) {
  if (java_ref_.is_uninitialized()) {
    video_size_ = natural_size;
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_updateVideoSize(
      env, java_ref_.get(env), natural_size.width(), natural_size.height());
}

void OverlayWindowAndroid::SetSurfaceId(const viz::SurfaceId& surface_id) {
  const viz::SurfaceId& old_surface_id = surface_layer_->surface_id().is_valid()
                                             ? surface_layer_->surface_id()
                                             : surface_id;
  surface_layer_->SetSurfaceId(surface_id,
                               cc::DeadlinePolicy::UseDefaultDeadline());

  if (!window_android_ || !window_android_->GetCompositor() ||
      old_surface_id == surface_id) {
    return;
  }

  window_android_->GetCompositor()->RemoveChildFrameSink(
      old_surface_id.frame_sink_id());
  window_android_->GetCompositor()->AddChildFrameSink(
      surface_id.frame_sink_id());
}

cc::Layer* OverlayWindowAndroid::GetLayerForTesting() {
  return nullptr;
}
