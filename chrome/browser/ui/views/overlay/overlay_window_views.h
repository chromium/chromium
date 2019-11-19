// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_

#include "content/public/browser/overlay_window.h"

#include "base/timer/timer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/rounded_corner_decorator.h"
#endif

namespace views {
class BackToTabImageButton;
class CloseImageButton;
class PlaybackImageButton;
class ResizeHandleButton;
class SkipAdLabelButton;
class TrackImageButton;
}  // namespace views

// The Chrome desktop implementation of OverlayWindow. This will only be
// implemented in views, which will support all desktop platforms.
class OverlayWindowViews : public content::OverlayWindow,
                           public views::ButtonListener,
                           public views::Widget {
 public:
  explicit OverlayWindowViews(
      content::PictureInPictureWindowController* controller);
  ~OverlayWindowViews() override;

  enum class WindowQuadrant { kBottomLeft, kBottomRight, kTopLeft, kTopRight };

  // OverlayWindow:
  bool IsActive() override;
  void Close() override;
  void ShowInactive() override;
  void Hide() override;
  bool IsVisible() override;
  bool IsAlwaysOnTop() override;
  gfx::Rect GetBounds() override;
  void UpdateVideoSize(const gfx::Size& natural_size) override;
  void SetPlaybackState(PlaybackState playback_state) override;
  void SetAlwaysHidePlayPauseButton(bool is_visible) override;
  void SetSkipAdButtonVisibility(bool is_visible) override;
  void SetNextTrackButtonVisibility(bool is_visible) override;
  void SetPreviousTrackButtonVisibility(bool is_visible) override;
  void SetSurfaceId(const viz::SurfaceId& surface_id) override;

  // views::Widget:
  bool IsActive() const override;
  bool IsVisible() const override;
  void OnNativeBlur() override;
  void OnNativeWidgetDestroyed() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetMove() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Gets the bounds of the controls.
  gfx::Rect GetBackToTabControlsBounds();
  gfx::Rect GetSkipAdControlsBounds();
  gfx::Rect GetCloseControlsBounds();
  gfx::Rect GetResizeHandleControlsBounds();
  gfx::Rect GetPlayPauseControlsBounds();
  gfx::Rect GetNextTrackControlsBounds();
  gfx::Rect GetPreviousTrackControlsBounds();

  // Gets the proper hit test component when the hit point is on the resize
  // handle in order to force a drag-to-resize.
  int GetResizeHTComponent() const;

  // Returns true if the controls (e.g. close button, play/pause button) are
  // visible.
  bool AreControlsVisible() const;

  views::PlaybackImageButton* play_pause_controls_view_for_testing() const;
  views::TrackImageButton* next_track_controls_view_for_testing() const;
  views::TrackImageButton* previous_track_controls_view_for_testing() const;
  views::SkipAdLabelButton* skip_ad_controls_view_for_testing() const;
  gfx::Point back_to_tab_image_position_for_testing() const;
  gfx::Point close_image_position_for_testing() const;
  gfx::Point resize_handle_position_for_testing() const;
  OverlayWindowViews::PlaybackState playback_state_for_testing() const;
  ui::Layer* video_layer_for_testing() const;
  cc::Layer* GetLayerForTesting() override;

  // Update the max size of the widget based on |work_area| and |window_size|.
  // The return value is the new size of the window if it was resized and is
  // only used for testing.
  gfx::Size UpdateMaxSize(const gfx::Rect& work_area,
                          const gfx::Size& window_size);

 private:
  // Return the work area for the nearest display the widget is on.
  gfx::Rect GetWorkAreaForWindow() const;

  // Determine the intended bounds of |this|. This should be called when there
  // is reason for the bounds to change, such as switching primary displays or
  // playing a new video (i.e. different aspect ratio). This also updates
  // |min_size_| and |max_size_|.
  gfx::Rect CalculateAndUpdateWindowBounds();

  // Set up the views::Views that will be shown on the window.
  void SetUpViews();

  // Update the bounds of the layers on the window. This may introduce
  // letterboxing.
  void UpdateLayerBoundsWithLetterboxing(gfx::Size window_size);

  // Updates the controls view::Views to reflect |is_visible|.
  void UpdateControlsVisibility(bool is_visible);

  // Updates the bounds of the controls.
  void UpdateControlsBounds();

  // Called when the bounds of the controls should be updated.
  void OnUpdateControlsBounds();

  // Update the size of each controls view as the size of the window changes.
  void UpdateButtonControlsSize();

  // Calculate and set the bounds of the controls.
  gfx::Rect CalculateControlsBounds(int x, const gfx::Size& size);
  void UpdateControlsPositions();

  ui::Layer* GetControlsScrimLayer();
  ui::Layer* GetBackToTabControlsLayer();
  ui::Layer* GetCloseControlsLayer();
  ui::Layer* GetResizeHandleLayer();
  ui::Layer* GetControlsParentLayer();

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class OverlayWindowControl {
    kBackToTab = 0,
    kMuteDeprecated,
    kSkipAd,
    kClose,
    kPlayPause,
    kNextTrack,
    kPreviousTrack,
    kMaxValue = kPreviousTrack
  };
  void RecordButtonPressed(OverlayWindowControl);
  void RecordTapGesture(OverlayWindowControl);

  // Toggles the play/pause control through the |controller_| and updates the
  // |play_pause_controls_view_| toggled state to reflect the current playing
  // state.
  void TogglePlayPause();

  // Returns the current frame sink id for the surface displayed in the
  // |video_view_]. If |video_view_| is not currently displaying a surface then
  // returns nullptr.
  const viz::FrameSinkId* GetCurrentFrameSinkId() const;

  // Not owned; |controller_| owns |this|.
  content::PictureInPictureWindowController* controller_;

  // Whether or not the components of the window has been set up. This is used
  // as a check as some event handlers (e.g. focus) is propogated to the window
  // before its contents is initialized. This is only set once.
  bool is_initialized_ = false;

  // Whether or not the window has been shown before. This is used to determine
  // sizing and placement. This is different from checking whether the window
  // components has been initialized.
  bool has_been_shown_ = false;

  // Whether or not the play/pause button will always be hidden. This is the
  // case for media streams video that user is not allowed to play/pause.
  bool always_hide_play_pause_button_ = false;

  // The upper and lower bounds of |current_size_|. These are determined by the
  // size of the primary display work area when Picture-in-Picture is initiated.
  // TODO(apacible): Update these bounds when the display the window is on
  // changes. http://crbug.com/819673
  gfx::Size min_size_;
  gfx::Size max_size_;

  // Current bounds of the Picture-in-Picture window.
  gfx::Rect window_bounds_;

  // Bounds of |video_view_|.
  gfx::Rect video_bounds_;

  // The natural size of the video to show. This is used to compute sizing and
  // ensuring factors such as aspect ratio is maintained.
  gfx::Size natural_size_;

  // Views to be shown.
  views::View* window_background_view_ = nullptr;
  views::View* video_view_ = nullptr;
  views::View* controls_scrim_view_ = nullptr;
  views::CloseImageButton* close_controls_view_ = nullptr;
  views::BackToTabImageButton* back_to_tab_controls_view_ = nullptr;
  views::TrackImageButton* previous_track_controls_view_ = nullptr;
  views::PlaybackImageButton* play_pause_controls_view_ = nullptr;
  views::TrackImageButton* next_track_controls_view_ = nullptr;
  views::SkipAdLabelButton* skip_ad_controls_view_ = nullptr;
  views::ResizeHandleButton* resize_handle_view_ = nullptr;
#if defined(OS_CHROMEOS)
  std::unique_ptr<ash::RoundedCornerDecorator> decorator_;
#endif

  // Automatically hides the controls a few seconds after user tap gesture.
  base::RetainingOneShotTimer hide_controls_timer_;

  // Timer used to update controls bounds.
  std::unique_ptr<base::OneShotTimer> update_controls_bounds_timer_;

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

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
