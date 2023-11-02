// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/playback_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

PlaybackImageButton::PlaybackImageButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  // Accessibility.
  const std::u16string playback_accessible_button_label(
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PLAY_PAUSE_CONTROL_ACCESSIBLE_TEXT));
  SetAccessibleName(playback_accessible_button_label);
}

void PlaybackImageButton::OnBoundsChanged(const gfx::Rect& rect) {
  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));
  play_image_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kPlayArrowIcon, kColorPipWindowForeground, icon_size);
  pause_image_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kPauseIcon, kColorPipWindowForeground, icon_size);
  replay_image_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kReplayIcon, kColorPipWindowForeground, icon_size);

  UpdateImageAndTooltipText();
}

void PlaybackImageButton::SetPlaybackState(
    const VideoOverlayWindowViews::PlaybackState playback_state) {
  if (playback_state_ == playback_state)
    return;

  playback_state_ = playback_state;
  UpdateImageAndTooltipText();
}

void PlaybackImageButton::UpdateImageAndTooltipText() {
  switch (playback_state_) {
    case VideoOverlayWindowViews::kPlaying:
      SetImageModel(views::Button::STATE_NORMAL, pause_image_);
      SetTooltipText(
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PAUSE_CONTROL_TEXT));
      break;
    case VideoOverlayWindowViews::kPaused:
      SetImageModel(views::Button::STATE_NORMAL, play_image_);
      SetTooltipText(
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PLAY_CONTROL_TEXT));
      break;
    case VideoOverlayWindowViews::kEndOfVideo:
      SetImageModel(views::Button::STATE_NORMAL, replay_image_);
      SetTooltipText(l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_REPLAY_CONTROL_TEXT));
      break;
  }
  SchedulePaint();
}

BEGIN_METADATA(PlaybackImageButton, OverlayWindowImageButton)
END_METADATA
