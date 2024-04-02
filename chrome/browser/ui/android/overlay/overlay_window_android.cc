// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/overlay/overlay_window_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/memory/ptr_util.h"
#include "cc/slim/surface_layer.h"
#include "chrome/android/chrome_jni_headers/PictureInPictureActivity_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "components/thin_webview/compositor_view.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android_compositor.h"

// static
std::unique_ptr<content::VideoOverlayWindow>
content::VideoOverlayWindow::Create(
    VideoPictureInPictureWindowController* controller) {
  return std::make_unique<OverlayWindowAndroid>(controller);
}

OverlayWindowAndroid::OverlayWindowAndroid(
    content::VideoPictureInPictureWindowController* controller)
    : window_android_(nullptr),
      compositor_view_(nullptr),
      surface_layer_(cc::slim::SurfaceLayer::Create()),
      bounds_(gfx::Rect(0, 0)),
      update_action_timer_(std::make_unique<base::OneShotTimer>()),
      controller_(controller) {
  surface_layer_->SetIsDrawable(true);
  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetBackgroundColor(SkColors::kBlack);

  auto* web_contents = controller_->GetWebContents();

  // Compute the screen position of the video, and see if it fits inside the
  // WebContents or if it's clipped / off-screen.  If it's onscreen, then T and
  // later, Android can do a nicer animated transition to PiP with a screen
  // capture of the video.  However, if the video is clipped / offscreen, then
  // it'll look nicer to use the default light grey transition.

  // We provide a small buffer for what "clipped" means, rather than enforcing
  // it strictly.  It'll still look fine while allowing small positioning errors
  // that sites sometimes make.  See https://crbug.com/1411517 for an example.

  // The java side will ignore any source bounds that are not on the screen for
  // the source rect hint. It will use the aspect ratio only in that case.  We
  // set the x position to be <0 to ensure this, to skip the transition.

  // Get the size of the video, and inset it to provide some slack.
  gfx::Rect source_bounds = controller_->GetSourceBounds();
  gfx::Rect smaller_source_bounds = source_bounds;
  constexpr int inset_size = 4;  // pixels on each side
  smaller_source_bounds.Inset(inset_size);

  // Get the size of the WebContents, and convert to pixels.
  gfx::Rect unscaled_content_bounds = web_contents->GetContainerBounds();
  auto* native_view = web_contents->GetNativeView();
  const float dip_scale = native_view->GetDipScale();
  gfx::Rect content_bounds(unscaled_content_bounds.x() * dip_scale,
                           unscaled_content_bounds.y() * dip_scale,
                           unscaled_content_bounds.width() * dip_scale,
                           unscaled_content_bounds.height() * dip_scale);
  const bool out_of_bounds = !content_bounds.Contains(smaller_source_bounds);

  if (!out_of_bounds) {
    // Use the newer transition, if available.
    // Convert to screen space.  Since the comparison was with the inset source
    // bounds, clamp the real source bounds to the container.
    source_bounds.Intersect(content_bounds);
    gfx::PointF offset = native_view->GetLocationOnScreen(0, 0);
    source_bounds.Offset(
        static_cast<int>(offset.x()),
        static_cast<int>(offset.y()) +
            native_view->content_offset() * native_view->GetDipScale());
  } else {
    // Use the old transition.
    // Slide this offscreen, while keeping the aspect ratio the same.
    source_bounds.set_x(-1);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_createActivity(
      env, reinterpret_cast<intptr_t>(this),
      TabAndroid::FromWebContents(web_contents)->GetJavaObject(),
      source_bounds.x(), source_bounds.y(), source_bounds.width(),
      source_bounds.height());
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

  Java_PictureInPictureActivity_setPlaybackState(env, java_ref_.get(env),
                                                 playback_state_);
  Java_PictureInPictureActivity_setMicrophoneMuted(env, java_ref_.get(env),
                                                   microphone_muted_);
  Java_PictureInPictureActivity_setCameraState(env, java_ref_.get(env),
                                               camera_on_);

  if (!update_action_timer_->IsRunning())
    MaybeNotifyVisibleActionsChanged();

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

  // Stop the timer for completeness, though resetting `java_ref_` will make it
  // a no-op.
  update_action_timer_->Stop();

  if (window_android_) {
    window_android_->RemoveObserver(this);
    window_android_ = nullptr;
  }

  // Only pause the video when play/pause button is visible.
  controller_->OnWindowDestroyed(
      /*should_pause_video=*/visible_actions_.find(
          static_cast<int>(media_session::mojom::MediaSessionAction::kPlay)) !=
      visible_actions_.end());
  // `this` may be destroyed.
}

void OverlayWindowAndroid::TogglePlayPause(JNIEnv* env, bool toggleOn) {
  DCHECK(!controller_->IsPlayerActive());
  if (toggleOn == (playback_state_ == PlaybackState::kPaused))
    controller_->TogglePlayPause();
}

void OverlayWindowAndroid::NextTrack(JNIEnv* env) {
  controller_->NextTrack();
}

void OverlayWindowAndroid::PreviousTrack(JNIEnv* env) {
  controller_->PreviousTrack();
}

void OverlayWindowAndroid::NextSlide(JNIEnv* env) {
  controller_->NextSlide();
}

void OverlayWindowAndroid::PreviousSlide(JNIEnv* env) {
  controller_->PreviousSlide();
}

void OverlayWindowAndroid::ToggleMicrophone(JNIEnv* env, bool toggleOn) {
  if (microphone_muted_ == toggleOn)
    controller_->ToggleMicrophone();
}

void OverlayWindowAndroid::ToggleCamera(JNIEnv* env, bool toggleOn) {
  if (!camera_on_ == toggleOn)
    controller_->ToggleCamera();
}

void OverlayWindowAndroid::HangUp(JNIEnv* env) {
  controller_->HangUp();
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

void OverlayWindowAndroid::OnBackToTab(JNIEnv* env) {
  controller_->FocusInitiator();
  Hide();
}

void OverlayWindowAndroid::Close() {
  CloseInternal();
  controller_->OnWindowDestroyed(/*should_pause_video=*/true);
}

void OverlayWindowAndroid::Hide() {
  CloseInternal();
  controller_->OnWindowDestroyed(/*should_pause_video=*/false);
  // `this` may be destroyed.
}

void OverlayWindowAndroid::CloseInternal() {
  if (java_ref_.is_uninitialized())
    return;

  DCHECK(window_android_);
  window_android_->RemoveObserver(this);
  window_android_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_close(env, java_ref_.get(env));

  // Stop any in-flight action button updates.  We won't find out if the Android
  // window is destroyed since that comes from `WindowAndroidObserver` but we
  // just unregistered from that.
  update_action_timer_->Stop();
}

bool OverlayWindowAndroid::IsActive() const {
  return true;
}

bool OverlayWindowAndroid::IsVisible() const {
  return true;
}

gfx::Rect OverlayWindowAndroid::GetBounds() {
  return bounds_;
}

void OverlayWindowAndroid::UpdateNaturalSize(const gfx::Size& natural_size) {
  if (java_ref_.is_uninitialized()) {
    video_size_ = natural_size;
    // This isn't guaranteed to be right, but it's better than (0,0).
    bounds_.set_size(natural_size);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_updateVideoSize(
      env, java_ref_.get(env), natural_size.width(), natural_size.height());
}

void OverlayWindowAndroid::SetPlaybackState(PlaybackState playback_state) {
  if (playback_state_ == playback_state)
    return;

  playback_state_ = playback_state;
  if (java_ref_.is_uninitialized())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_setPlaybackState(env, java_ref_.get(env),
                                                 playback_state);
}

void OverlayWindowAndroid::SetMicrophoneMuted(bool muted) {
  if (microphone_muted_ == muted)
    return;

  microphone_muted_ = muted;
  if (java_ref_.is_uninitialized())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_setMicrophoneMuted(env, java_ref_.get(env),
                                                   microphone_muted_);
}

void OverlayWindowAndroid::SetCameraState(bool turned_on) {
  if (camera_on_ == turned_on)
    return;

  camera_on_ = turned_on;
  if (java_ref_.is_uninitialized())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_setCameraState(env, java_ref_.get(env),
                                               camera_on_);
}

void OverlayWindowAndroid::SetPlayPauseButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(media_session::mojom::MediaSessionAction::kPlay,
                           is_visible);
}

void OverlayWindowAndroid::SetNextTrackButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(media_session::mojom::MediaSessionAction::kNextTrack,
                           is_visible);
}

void OverlayWindowAndroid::SetPreviousTrackButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(
      media_session::mojom::MediaSessionAction::kPreviousTrack, is_visible);
}

void OverlayWindowAndroid::SetToggleMicrophoneButtonVisibility(
    bool is_visible) {
  MaybeUpdateVisibleAction(
      media_session::mojom::MediaSessionAction::kToggleMicrophone, is_visible);
}

void OverlayWindowAndroid::SetToggleCameraButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(
      media_session::mojom::MediaSessionAction::kToggleCamera, is_visible);
}

void OverlayWindowAndroid::SetHangUpButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(media_session::mojom::MediaSessionAction::kHangUp,
                           is_visible);
}

void OverlayWindowAndroid::SetNextSlideButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(media_session::mojom::MediaSessionAction::kNextSlide,
                           is_visible);
}

void OverlayWindowAndroid::SetPreviousSlideButtonVisibility(bool is_visible) {
  MaybeUpdateVisibleAction(
      media_session::mojom::MediaSessionAction::kPreviousSlide, is_visible);
}

void OverlayWindowAndroid::SetSurfaceId(const viz::SurfaceId& surface_id) {
  const viz::SurfaceId& old_surface_id = surface_layer_->surface_id().is_valid()
                                             ? surface_layer_->surface_id()
                                             : surface_id;
  if (window_android_ && window_android_->GetCompositor() &&
      old_surface_id.frame_sink_id() != surface_id.frame_sink_id()) {
    // On Android, the new frame sink needs to be added before
    // removing the previous surface sink.
    window_android_->GetCompositor()->AddChildFrameSink(
        surface_id.frame_sink_id());
    window_android_->GetCompositor()->RemoveChildFrameSink(
        old_surface_id.frame_sink_id());
  }
  // Set the surface after frame sink hierarchy update.
  surface_layer_->SetSurfaceId(surface_id,
                               cc::DeadlinePolicy::UseDefaultDeadline());
}

void OverlayWindowAndroid::MaybeNotifyVisibleActionsChanged() {
  if (java_ref_.is_uninitialized())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PictureInPictureActivity_updateVisibleActions(
      env, java_ref_.get(env),
      base::android::ToJavaIntArray(
          env,
          std::vector<int>(visible_actions_.begin(), visible_actions_.end())));
}

void OverlayWindowAndroid::MaybeUpdateVisibleAction(
    const media_session::mojom::MediaSessionAction& action,
    bool is_visible) {
  int action_code = static_cast<int>(action);
  if ((visible_actions_.find(action_code) != visible_actions_.end()) ==
      is_visible) {
    return;
  }

  if (is_visible)
    visible_actions_.insert(action_code);
  else
    visible_actions_.erase(action_code);

  if (!update_action_timer_->IsRunning()) {
    update_action_timer_->Start(
        FROM_HERE, base::Seconds(1),
        base::BindOnce(&OverlayWindowAndroid::MaybeNotifyVisibleActionsChanged,
                       base::Unretained(this)));
  }
}
