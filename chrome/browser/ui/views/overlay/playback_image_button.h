// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_PLAYBACK_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_PLAYBACK_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"

namespace views {

// A resizable playback button with 3 states: play/pause/replay.
class PlaybackImageButton : public views::ImageButton {
 public:
  explicit PlaybackImageButton(ButtonListener*);
  ~PlaybackImageButton() override = default;

  // Show appropriate images based on playback state.
  void SetPlaybackState(const OverlayWindowViews::PlaybackState playback_state);

 protected:
  // Overridden from views::View.
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  void UpdateImageAndTooltipText();

  OverlayWindowViews::PlaybackState playback_state_ =
      OverlayWindowViews::PlaybackState::kEndOfVideo;

  gfx::ImageSkia play_image_;
  gfx::ImageSkia pause_image_;
  gfx::ImageSkia replay_image_;

  DISALLOW_COPY_AND_ASSIGN(PlaybackImageButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_PLAYBACK_IMAGE_BUTTON_H_
