// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_PLAYBACK_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_PLAYBACK_IMAGE_BUTTON_H_

#include <optional>

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"

// A resizable playback button with 3 states: play/pause/replay.
class PlaybackImageButton : public OverlayWindowImageButton {
  METADATA_HEADER(PlaybackImageButton, OverlayWindowImageButton)

 public:
  explicit PlaybackImageButton(PressedCallback callback);
  PlaybackImageButton(const PlaybackImageButton&) = delete;
  PlaybackImageButton& operator=(const PlaybackImageButton&) = delete;
  ~PlaybackImageButton() override = default;

  // Show appropriate images based on playback state.
  void SetPlaybackState(
      const VideoOverlayWindowViews::PlaybackState playback_state);

  // Updates the position of this button within the new bounds of the window.
  void SetWindowSize(const gfx::Size& window_size);

 protected:
  // Overridden from views::View.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImageAndText();
  void UpdatePosition();

  void SetPlayButtonBackground();
  void SetPauseButtonBackground();

  std::optional<gfx::Size> window_size_;

  VideoOverlayWindowViews::PlaybackState playback_state_ =
      VideoOverlayWindowViews::PlaybackState::kEndOfVideo;

  ui::ImageModel play_image_;
  ui::ImageModel pause_image_;
  ui::ImageModel replay_image_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_PLAYBACK_IMAGE_BUTTON_H_
