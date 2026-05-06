// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/overlay/overlay_window_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/unguessable_token_android.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "cc/slim/surface_layer.h"
#include "chrome/android/chrome_jni_headers/VideoOverlayActivity_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"
#include "components/thin_webview/compositor_view.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "ui/android/window_android_compositor.h"

using WindowMap = base::flat_map<base::UnguessableToken, OverlayWindowAndroid*>;

namespace {
WindowMap& GetWindowMap() {
  static base::NoDestructor<WindowMap> instance;
  return *instance;
}

}  // namespace

// static
OverlayWindowAndroid* OverlayWindowAndroid::FromToken(
    const base::UnguessableToken& token) {
  auto iter = GetWindowMap().find(token);
  return (iter == GetWindowMap().end()) ? nullptr : iter->second;
}

OverlayWindowAndroid::OverlayWindowAndroid(
    content::VideoPictureInPictureWindowController* controller)
    : window_android_(nullptr),
      compositor_view_(nullptr),
      surface_layer_(cc::slim::SurfaceLayer::Create()),
      bounds_(gfx::Rect(0, 0)),
      update_action_timer_(std::make_unique<base::OneShotTimer>()),
      controller_(controller) {
  GetWindowMap().emplace(token_, this);
  surface_layer_->SetIsDrawable(true);
  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetBackgroundColor(SkColors::kBlack);
}

OverlayWindowAndroid::~OverlayWindowAndroid() {
  // Any future use of our token will fail.
  GetWindowMap().erase(token_);
}

void OverlayWindowAndroid::Initialize(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& self,
    const base::android::JavaRef<jobject>& jwindow_android) {
  java_ref_ = JavaObjectWeakGlobalRef(env, self);
  window_android_ = ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  window_android_->AddObserver(this);

  SetPlaybackStateJava(playback_state_);
  SetMicrophoneMutedJava(microphone_muted_);
  SetCameraStateJava(camera_on_);

  if (!update_action_timer_->IsRunning()) {
    MaybeNotifyVisibleActionsChanged();
  }

  if (!video_size_.IsEmpty()) {
    UpdateVideoSizeJava(video_size_.width(), video_size_.height());
  }

  if (media_position_.has_value()) {
    SetMediaPositionJava(media_position_.value());
  }
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
  if (java_ref_.is_uninitialized()) {
    return;
  }

  // If the activity stops, pretend that somebody pressed the close button.
  // This will notify the java side to forget about us, and clean up.
  Close();
  // `this` may be destroyed.
}

void OverlayWindowAndroid::DestroyStartedByJava(JNIEnv* env) {
  // Note that the java side also clears its native ptr when calling us, so it's
  // okay that we don't notify it in the dtor.
  // ** IMPORTANT ** Do not add calls here unless the above statement continues
  // to be true.  It's unlikely that anything on the native side should call
  // this method directly.
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
  bool is_paused_or_ended = playback_state_ == PlaybackState::kPaused ||
                            playback_state_ == PlaybackState::kEndOfVideo;
  if (toggleOn == is_paused_or_ended) {
    controller_->TogglePlayPause();
  }
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
  if (microphone_muted_ == toggleOn) {
    controller_->ToggleMicrophone();
  }
}

void OverlayWindowAndroid::ToggleCamera(JNIEnv* env, bool toggleOn) {
  if (!camera_on_ == toggleOn) {
    controller_->ToggleCamera();
  }
}

void OverlayWindowAndroid::HangUp(JNIEnv* env) {
  controller_->HangUp();
}

void OverlayWindowAndroid::SeekTo(JNIEnv* env, int64_t position_ms) {
  controller_->SeekTo(base::Milliseconds(position_ms));
}

void OverlayWindowAndroid::Hide(JNIEnv* env) {
  if (auto* web_contents = controller_->GetWebContents()) {
    if (auto* helper =
            AutoPictureInPictureTabHelper::FromWebContents(web_contents)) {
      helper->OnPictureInPictureWindowWillHide();
    }
  }

  // Hides the window without pausing the video, effectively moving the playback
  // to the background.
  Hide();
}

void OverlayWindowAndroid::CompositorViewCreated(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& compositor_view) {
  compositor_view_ =
      thin_webview::android::CompositorView::FromJavaObject(compositor_view);
  DCHECK(compositor_view_);
  compositor_view_->SetRootLayer(surface_layer_);
}

void OverlayWindowAndroid::OnViewSizeChanged(JNIEnv* env,
                                             int32_t width,
                                             int32_t height) {
  gfx::Size content_size(width, height);
  if (bounds_.size() == content_size) {
    return;
  }

  bounds_.set_size(content_size);
  surface_layer_->SetBounds(content_size);
  controller_->UpdateLayerBounds();
}

void OverlayWindowAndroid::OnBackToTab(JNIEnv* env) {
  if (base::FeatureList::IsEnabled(media::kAutoPictureInPictureAndroid)) {
    // Call `CloseAndFocusInitiator()` here to prevent a race condition with the
    // auto-PiP flow. The race occurs between two paths trying to close the
    // window:
    // 1. This manual "back to tab" action.
    // 2. The `AutoPictureInPictureTabHelper`, which automatically closes the
    //    window when the originating tab becomes visible.
    //
    // If we were to call `FocusInitiator()` first, it would make the tab
    // visible, triggering path (2) immediately. This `OnBackToTab` flow (path
    // 1) would then also try to close the window, leading to a use-after-free
    // crash.
    //
    // `CloseAndFocusInitiator()` solves this by ensuring the controller
    // destroys the window (`Close()`) *before* focusing the tab
    // (`FocusInitiator()`). This way, by the time the tab helper in path (2)
    // runs, the window is already gone, and its attempt to close it becomes a
    // safe no-op.
    controller_->CloseAndFocusInitiator();
  } else {
    controller_->FocusInitiator();
    Hide();
  }
}

void OverlayWindowAndroid::OnDismissal(JNIEnv* env) {
  // Verify that the dismissal is for an auto-PiP session, not a user-initiated
  // one, before triggering the embargo logic.
  if (IsInAutoPictureInPicture()) {
    AutoPictureInPictureTabHelper::FromWebContents(
        controller_->GetWebContents())
        ->OnPictureInPictureDismissed();
  }
}

static int64_t JNI_VideoOverlayActivity_OnActivityStart(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_token,
    const jni_zero::JavaRef<jobject>& self,
    const jni_zero::JavaRef<jobject>& window) {
  auto token = base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(
      env, j_token);
  OverlayWindowAndroid* thiz = OverlayWindowAndroid::FromToken(token);
  if (!thiz) {
    return 0;
  }
  thiz->Initialize(env, self, window);
  return reinterpret_cast<int64_t>(thiz);
}

void OverlayWindowAndroid::OnWindowDestroyedJava() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_onWindowDestroyed(env, obj);
}

void OverlayWindowAndroid::UpdateVideoSizeJava(int width, int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_updateVideoSize(env, obj, width, height);
}

void OverlayWindowAndroid::SetPlaybackStateJava(PlaybackState playback_state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_setPlaybackState(env, obj,
                                             static_cast<int>(playback_state));
}

void OverlayWindowAndroid::CloseJava() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_close(env, obj);
}

void OverlayWindowAndroid::UpdateVisibleActionsJava(
    const std::vector<int>& actions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_updateVisibleActions(
      env, obj, base::android::ToJavaIntArray(env, actions));
}

void OverlayWindowAndroid::SetMicrophoneMutedJava(bool muted) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_setMicrophoneMuted(env, obj, muted);
}

void OverlayWindowAndroid::SetCameraStateJava(bool turned_on) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_setCameraState(env, obj, turned_on);
}

void OverlayWindowAndroid::SetMediaPositionJava(
    const media_session::MediaPosition& position) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_setMediaPosition(
      env, obj, position.duration().InMilliseconds(),
      position.GetPosition().InMilliseconds(), position.playback_rate());
}

void OverlayWindowAndroid::SetImmersiveVideoOptionsJava(
    const blink::mojom::ImmersiveOptionsPtr& immersive_options) {
  if (!immersive_options) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    return;
  }
  Java_VideoOverlayActivity_setImmersiveVideoOptions(
      env, obj, static_cast<int>(immersive_options->stereo_mode),
      static_cast<int>(immersive_options->projection_type));
}

void OverlayWindowAndroid::Close() {
  CloseInternal();
  // Only pause the video when play/pause button is visible.
  controller_->OnWindowDestroyed(
      /*should_pause_video=*/visible_actions_.find(
          static_cast<int>(media_session::mojom::MediaSessionAction::kPlay)) !=
      visible_actions_.end());
  // `this` may be destroyed.
}

void OverlayWindowAndroid::Hide() {
  CloseInternal();
  controller_->OnWindowDestroyed(/*should_pause_video=*/false);
  // `this` may be destroyed.
}

void OverlayWindowAndroid::CloseInternal() {
  if (java_ref_.is_uninitialized()) {
    return;
  }

  DCHECK(window_android_);
  window_android_->RemoveObserver(this);
  window_android_ = nullptr;
  CloseJava();
  // The java side forgets about us on close, so don't call back.
  java_ref_.reset();

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

bool OverlayWindowAndroid::IsInAutoPictureInPicture() const {
  auto* web_contents = controller_->GetWebContents();
  if (!web_contents) {
    return false;
  }

  auto* helper = AutoPictureInPictureTabHelper::FromWebContents(web_contents);
  return helper && helper->IsInAutoPictureInPicture();
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
  UpdateVideoSizeJava(natural_size.width(), natural_size.height());
}

void OverlayWindowAndroid::SetPlaybackState(PlaybackState playback_state) {
  if (playback_state_ == playback_state) {
    return;
  }

  playback_state_ = playback_state;
  SetPlaybackStateJava(playback_state);
}

void OverlayWindowAndroid::SetMediaPosition(
    const media_session::MediaPosition& position) {
  media_position_ = position;
  SetMediaPositionJava(position);
}

void OverlayWindowAndroid::SetMicrophoneMuted(bool muted) {
  if (microphone_muted_ == muted) {
    return;
  }

  microphone_muted_ = muted;
  SetMicrophoneMutedJava(microphone_muted_);
}

void OverlayWindowAndroid::SetCameraState(bool turned_on) {
  if (camera_on_ == turned_on) {
    return;
  }

  camera_on_ = turned_on;
  SetCameraStateJava(camera_on_);
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

void OverlayWindowAndroid::SetHidePictureInPictureButtonVisibility(
    bool is_visible) {
  // The hide button is only shown for auto-PiP sessions. The controller
  // provides `is_visible` as a hint based on play/pause visibility, but we make
  // the final decision here based on the auto-PiP state.
  bool should_show_hide_button = is_visible && IsInAutoPictureInPicture();
  MaybeUpdateVisibleAction(
      media_session::mojom::MediaSessionAction::kExitPictureInPicture,
      should_show_hide_button);
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
  if (java_ref_.is_uninitialized()) {
    return;
  }
  UpdateVisibleActionsJava(
      std::vector<int>(visible_actions_.begin(), visible_actions_.end()));
}

void OverlayWindowAndroid::MaybeUpdateVisibleAction(
    const media_session::mojom::MediaSessionAction& action,
    bool is_visible) {
  int action_code = static_cast<int>(action);
  if ((visible_actions_.find(action_code) != visible_actions_.end()) ==
      is_visible) {
    return;
  }

  if (is_visible) {
    visible_actions_.insert(action_code);
  } else {
    visible_actions_.erase(action_code);
  }

  if (!update_action_timer_->IsRunning()) {
    update_action_timer_->Start(
        FROM_HERE, base::Seconds(1),
        base::BindOnce(&OverlayWindowAndroid::MaybeNotifyVisibleActionsChanged,
                       base::Unretained(this)));
  }
}

DEFINE_JNI(VideoOverlayActivity)
