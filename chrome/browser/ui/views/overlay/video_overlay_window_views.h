// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "chromeos/ui/frame/highlight_border_overlay.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace viz {
class FrameSinkId;
}  // namespace viz

class BackToTabImageButton;
class BackToTabLabelButton;
class CloseImageButton;
class HangUpButton;
class PlaybackImageButton;
class ResizeHandleButton;
class SkipAdLabelButton;
class ToggleMicrophoneButton;
class ToggleCameraButton;
class TrackImageButton;

// The Chrome desktop implementation of OverlayWindow. This will only be
// implemented in views, which will support all desktop platforms.
class VideoOverlayWindowViews : public OverlayWindowViews,
                                public content::VideoOverlayWindow {
 public:
  static std::unique_ptr<VideoOverlayWindowViews> Create(
      content::VideoPictureInPictureWindowController* controller);

  VideoOverlayWindowViews(const VideoOverlayWindowViews&) = delete;
  VideoOverlayWindowViews& operator=(const VideoOverlayWindowViews&) = delete;

  ~VideoOverlayWindowViews() override;

  // OverlayWindow:
  bool IsActive() override;
  void Close() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() override;
  bool IsAlwaysOnTop() override;
  gfx::Rect GetBounds() override;
  void UpdateNaturalSize(const gfx::Size& natural_size) override;

  // VideoOverlayWindow:
  void SetPlaybackState(PlaybackState playback_state) override;
  void SetPlayPauseButtonVisibility(bool is_visible) override;
  void SetSkipAdButtonVisibility(bool is_visible) override;
  void SetNextTrackButtonVisibility(bool is_visible) override;
  void SetPreviousTrackButtonVisibility(bool is_visible) override;
  void SetMicrophoneMuted(bool muted) override;
  void SetCameraState(bool turned_on) override;
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override;
  void SetToggleCameraButtonVisibility(bool is_visible) override;
  void SetHangUpButtonVisibility(bool is_visible) override;
  void SetSurfaceId(const viz::SurfaceId& surface_id) override;
  cc::Layer* GetLayerForTesting() override;

  // OverlayWindowViews
  bool ControlsHitTestContainsPoint(const gfx::Point& point) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  int GetResizeHTComponent() const override;
  gfx::Rect GetResizeHandleControlsBounds() override;
  void UpdateResizeHandleBounds(WindowQuadrant quadrant) override;
#endif
  void OnUpdateControlsBounds() override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void SetUpViews() override;
  void OnRootViewReady() override;
  void UpdateLayerBoundsWithLetterboxing(gfx::Size window_size) override;
  content::PictureInPictureWindowController* GetController() const override;
  views::View* GetWindowBackgroundView() const override;
  views::View* GetControlsContainerView() const override;

  // views::Widget
  bool IsActive() const override;
  bool IsVisible() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetDestroying() override;
  void OnNativeWidgetDestroyed() override;
  void OnNativeWidgetAddedToCompositor() override;
  void OnNativeWidgetRemovingFromCompositor() override;

  // Gets the bounds of the controls.
  gfx::Rect GetBackToTabControlsBounds();
  gfx::Rect GetSkipAdControlsBounds();
  gfx::Rect GetCloseControlsBounds();
  gfx::Rect GetPlayPauseControlsBounds();
  gfx::Rect GetNextTrackControlsBounds();
  gfx::Rect GetPreviousTrackControlsBounds();
  gfx::Rect GetToggleMicrophoneButtonBounds();
  gfx::Rect GetToggleCameraButtonBounds();
  gfx::Rect GetHangUpButtonBounds();

  PlaybackImageButton* play_pause_controls_view_for_testing() const;
  TrackImageButton* next_track_controls_view_for_testing() const;
  TrackImageButton* previous_track_controls_view_for_testing() const;
  SkipAdLabelButton* skip_ad_controls_view_for_testing() const;
  ToggleMicrophoneButton* toggle_microphone_button_for_testing() const;
  ToggleCameraButton* toggle_camera_button_for_testing() const;
  HangUpButton* hang_up_button_for_testing() const;
  BackToTabLabelButton* back_to_tab_label_button_for_testing() const;
  CloseImageButton* close_button_for_testing() const;
  gfx::Point close_image_position_for_testing() const;
  gfx::Point resize_handle_position_for_testing() const;
  PlaybackState playback_state_for_testing() const;
  ui::Layer* video_layer_for_testing() const;

 protected:
  explicit VideoOverlayWindowViews(
      content::VideoPictureInPictureWindowController* controller);

 private:
  // Calculate and set the bounds of the controls.
  gfx::Rect CalculateControlsBounds(int x, const gfx::Size& size);

  // Toggles the play/pause control through the |controller_| and updates the
  // |play_pause_controls_view_| toggled state to reflect the current playing
  // state.
  void TogglePlayPause();

  // Returns the current frame sink id for the surface displayed in the
  // |video_view_|. If |video_view_| is not currently displaying a surface then
  // returns nullptr.
  const viz::FrameSinkId* GetCurrentFrameSinkId() const;

  // Unregisters the current frame sink id for the surface displayed in the
  // |video_view_| from its parent frame sink if the frame sink hierarchy has
  // been registered before.
  void MaybeUnregisterFrameSinkHierarchy();

  // Not owned; |controller_| owns |this|.
  raw_ptr<content::VideoPictureInPictureWindowController> controller_;

  // Whether or not the play/pause button will be shown.
  bool show_play_pause_button_ = false;

  // Temporary storage for child Views. Used during the time between
  // construction and initialization, when the views::View pointer members must
  // already be initialized, but there is no root view to add them to yet.
  std::vector<std::unique_ptr<views::View>> view_holder_;

  // Views to be shown. The views are first temporarily owned by view_holder_,
  // then passed to this widget's ContentsView which takes ownership.
  raw_ptr<views::View> window_background_view_ = nullptr;
  raw_ptr<views::View> video_view_ = nullptr;
  raw_ptr<views::View> controls_scrim_view_ = nullptr;
  raw_ptr<views::View> controls_container_view_ = nullptr;
  raw_ptr<CloseImageButton> close_controls_view_ = nullptr;
  raw_ptr<BackToTabImageButton> back_to_tab_image_button_ = nullptr;
  raw_ptr<BackToTabLabelButton> back_to_tab_label_button_ = nullptr;
  raw_ptr<TrackImageButton> previous_track_controls_view_ = nullptr;
  raw_ptr<PlaybackImageButton> play_pause_controls_view_ = nullptr;
  raw_ptr<TrackImageButton> next_track_controls_view_ = nullptr;
  raw_ptr<SkipAdLabelButton> skip_ad_controls_view_ = nullptr;
  raw_ptr<ResizeHandleButton> resize_handle_view_ = nullptr;
  raw_ptr<ToggleMicrophoneButton> toggle_microphone_button_ = nullptr;
  raw_ptr<ToggleCameraButton> toggle_camera_button_ = nullptr;
  raw_ptr<HangUpButton> hang_up_button_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Generates a nine patch layer painted with a highlight border for ChromeOS
  // ASH only, not including LaCrOS. Highlight border for chrome pip window in
  // LaCrOS will be added when `ash::NonClientFrameViewAsh` is created.
  std::unique_ptr<HighlightBorderOverlay> highlight_border_overlay_;
#endif

  // Current playback state on the video in Picture-in-Picture window. It is
  // used to toggle play/pause/replay button.
  PlaybackState playback_state_for_testing_ = kEndOfVideo;

  // Whether or not the skip ad button will be shown. This is the
  // case when Media Session "skipad" action is handled by the website.
  bool show_skip_ad_button_ = false;

  // Whether or not the next track button will be shown. This is the
  // case when Media Session "nexttrack" action is handled by the website.
  bool show_next_track_button_ = false;

  // Whether or not the previous track button will be shown. This is the
  // case when Media Session "previoustrack" action is handled by the website.
  bool show_previous_track_button_ = false;

  // Whether or not the toggle microphone button will be shown. This is the case
  // when Media Session "togglemicrophone" action is handled by the website.
  bool show_toggle_microphone_button_ = false;

  // Whether or not the toggle camera button will be shown. This is the case
  // when Media Session "togglecamera" action is handled by the website.
  bool show_toggle_camera_button_ = false;

  // Whether or not the hang up button will be shown. This is the case when
  // Media Session "hangup" action is handled by the website.
  bool show_hang_up_button_ = false;

  // Whether or not the current frame sink for the surface displayed in the
  // |video_view_| is registered as the child of the overlay window frame sink.
  bool has_registered_frame_sink_hierarchy_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_VIDEO_OVERLAY_WINDOW_VIEWS_H_
