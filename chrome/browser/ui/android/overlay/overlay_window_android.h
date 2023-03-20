// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "content/public/browser/overlay_window.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_observer.h"
#include "ui/gfx/geometry/size.h"

namespace cc::slim {
class SurfaceLayer;
}

namespace thin_webview {
namespace android {
class CompositorView;
}  // namespace android
}  // namespace thin_webview

class OverlayWindowAndroid : public content::VideoOverlayWindow,
                             public ui::WindowAndroidObserver {
 public:
  explicit OverlayWindowAndroid(
      content::VideoPictureInPictureWindowController* controller);
  ~OverlayWindowAndroid() override;

  void OnActivityStart(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jwindow_android);
  void Destroy(JNIEnv* env);
  void TogglePlayPause(JNIEnv* env, bool toggleOn);
  void NextTrack(JNIEnv* env);
  void PreviousTrack(JNIEnv* env);
  void NextSlide(JNIEnv* env);
  void PreviousSlide(JNIEnv* env);
  void ToggleMicrophone(JNIEnv* env, bool toggleOn);
  void ToggleCamera(JNIEnv* env, bool toggleOn);
  void HangUp(JNIEnv* env);
  void CompositorViewCreated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& compositor_view);
  void OnViewSizeChanged(JNIEnv* env, jint width, jint height);
  void OnBackToTab(JNIEnv* env);

  // ui::WindowAndroidObserver implementation.
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override;
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks frame_begin_time) override {}
  void OnActivityStopped() override;
  void OnActivityStarted() override {}

  // VideoOverlayWindow implementation.
  bool IsActive() const override;
  void Close() override;
  void ShowInactive() override {}
  void Hide() override;
  bool IsVisible() const override;
  gfx::Rect GetBounds() override;
  void UpdateNaturalSize(const gfx::Size& natural_size) override;
  void SetPlaybackState(PlaybackState playback_state) override;
  void SetPlayPauseButtonVisibility(bool is_visible) override;
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override;
  void SetPreviousTrackButtonVisibility(bool is_visible) override;
  void SetMicrophoneMuted(bool muted) override;
  void SetCameraState(bool turned_on) override;
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override;
  void SetToggleCameraButtonVisibility(bool is_visible) override;
  void SetHangUpButtonVisibility(bool is_visible) override;
  void SetNextSlideButtonVisibility(bool is_visible) override;
  void SetPreviousSlideButtonVisibility(bool is_visible) override;
  void SetSurfaceId(const viz::SurfaceId& surface_id) override;

 private:
  // Notify PictureInPictureActivity that visible actions have changed.
  void MaybeNotifyVisibleActionsChanged();

  // Maybe update visible actions. Returns true if update happened.
  void MaybeUpdateVisibleAction(
      const media_session::mojom::MediaSessionAction& action,
      bool is_visible);
  void CloseInternal();

  // A weak reference to Java PictureInPictureActivity object.
  JavaObjectWeakGlobalRef java_ref_;
  raw_ptr<ui::WindowAndroid> window_android_;
  raw_ptr<thin_webview::android::CompositorView> compositor_view_;
  scoped_refptr<cc::slim::SurfaceLayer> surface_layer_;
  gfx::Rect bounds_;
  gfx::Size video_size_;

  PlaybackState playback_state_ = PlaybackState::kEndOfVideo;
  std::unordered_set<int> visible_actions_;

  bool microphone_muted_ = false;
  bool camera_on_ = false;

  std::unique_ptr<base::OneShotTimer> update_action_timer_;

  raw_ptr<content::VideoPictureInPictureWindowController> controller_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_OVERLAY_OVERLAY_WINDOW_ANDROID_H_
