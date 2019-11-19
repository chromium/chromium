// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/playback_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

SkColor kPlaybackIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

PlaybackImageButton::PlaybackImageButton(ButtonListener* listener)
    : ImageButton(listener) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 playback_accessible_button_label(
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PLAY_PAUSE_CONTROL_ACCESSIBLE_TEXT));
  SetAccessibleName(playback_accessible_button_label);
  SetInstallFocusRingOnFocus(true);
}

void PlaybackImageButton::OnBoundsChanged(const gfx::Rect& rect) {
  play_image_ = gfx::CreateVectorIcon(vector_icons::kPlayArrowIcon,
                                      size().width(), kPlaybackIconColor);
  pause_image_ = gfx::CreateVectorIcon(vector_icons::kPauseIcon, size().width(),
                                       kPlaybackIconColor);
  replay_image_ = gfx::CreateVectorIcon(vector_icons::kReplayIcon,
                                        size().width(), kPlaybackIconColor);

  UpdateImageAndTooltipText();
}

void PlaybackImageButton::SetPlaybackState(
    const OverlayWindowViews::PlaybackState playback_state) {
  if (playback_state_ == playback_state)
    return;

  playback_state_ = playback_state;
  UpdateImageAndTooltipText();
}

void PlaybackImageButton::UpdateImageAndTooltipText() {
  switch (playback_state_) {
    case OverlayWindowViews::kPlaying:
      SetImage(views::Button::STATE_NORMAL, pause_image_);
      SetTooltipText(
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PAUSE_CONTROL_TEXT));
      break;
    case OverlayWindowViews::kPaused:
      SetImage(views::Button::STATE_NORMAL, play_image_);
      SetTooltipText(
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PLAY_CONTROL_TEXT));
      break;
    case OverlayWindowViews::kEndOfVideo:
      SetImage(views::Button::STATE_NORMAL, replay_image_);
      SetTooltipText(l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_REPLAY_CONTROL_TEXT));
      break;
  }
  SchedulePaint();
}

}  // namespace views
