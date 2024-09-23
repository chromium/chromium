// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/playback_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kPlaybackButtonSize = 48;
constexpr int kPlaybackButtonIconSize = 24;

}  // namespace

PlaybackImageButton::PlaybackImageButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  // For the 2024 updated UI, we're in charge of our own size, and the icons
  // never change.
  if (base::FeatureList::IsEnabled(
          media::kVideoPictureInPictureControlsUpdate2024)) {
    SetSize(gfx::Size(kPlaybackButtonSize, kPlaybackButtonSize));

    play_image_ = ui::ImageModel::FromVectorIcon(
        vector_icons::kPlayArrowIcon, ui::kColorSysOnSecondaryContainer,
        kPlaybackButtonIconSize);
    pause_image_ = ui::ImageModel::FromVectorIcon(vector_icons::kPauseIcon,
                                                  kColorPipWindowForeground,
                                                  kPlaybackButtonIconSize);
    replay_image_ = ui::ImageModel::FromVectorIcon(
        vector_icons::kReplayIcon, ui::kColorSysOnSecondaryContainer,
        kPlaybackButtonIconSize);

    UpdateImageAndText();
  }

  // Accessibility.
  const std::u16string playback_accessible_button_label(
      l10n_util::GetStringUTF16(
          IDS_PICTURE_IN_PICTURE_PLAY_PAUSE_CONTROL_ACCESSIBLE_TEXT));
  GetViewAccessibility().SetName(playback_accessible_button_label);
}

void PlaybackImageButton::OnBoundsChanged(const gfx::Rect& rect) {
  if (base::FeatureList::IsEnabled(
          media::kVideoPictureInPictureControlsUpdate2024)) {
    return;
  }

  int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));
  play_image_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kPlayArrowIcon, kColorPipWindowForeground, icon_size);
  pause_image_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kPauseIcon, kColorPipWindowForeground, icon_size);
  replay_image_ = ui::ImageModel::FromVectorIcon(
      vector_icons::kReplayIcon, kColorPipWindowForeground, icon_size);

  UpdateImageAndText();
}

void PlaybackImageButton::SetPlaybackState(
    const VideoOverlayWindowViews::PlaybackState playback_state) {
  if (playback_state_ == playback_state)
    return;

  playback_state_ = playback_state;
  UpdateImageAndText();
}

void PlaybackImageButton::SetWindowSize(const gfx::Size& window_size) {
  if (window_size_.has_value() && window_size_.value() == window_size) {
    return;
  }

  window_size_ = window_size;
  UpdatePosition();
}

void PlaybackImageButton::UpdateImageAndText() {
  switch (playback_state_) {
    case VideoOverlayWindowViews::kPlaying: {
      SetImageModel(views::Button::STATE_NORMAL, pause_image_);
      SetPauseButtonBackground();
      std::u16string pause_text =
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PAUSE_CONTROL_TEXT);
      SetTooltipText(pause_text);
      GetViewAccessibility().SetName(pause_text);
      break;
    }
    case VideoOverlayWindowViews::kPaused: {
      SetImageModel(views::Button::STATE_NORMAL, play_image_);
      SetPlayButtonBackground();
      std::u16string play_text =
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_PLAY_CONTROL_TEXT);
      SetTooltipText(play_text);
      GetViewAccessibility().SetName(play_text);
      break;
    }
    case VideoOverlayWindowViews::kEndOfVideo: {
      SetImageModel(views::Button::STATE_NORMAL, replay_image_);
      SetPlayButtonBackground();
      std::u16string replay_text =
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_REPLAY_CONTROL_TEXT);
      SetTooltipText(replay_text);
      GetViewAccessibility().SetName(replay_text);
      break;
    }
  }

  SchedulePaint();
}

void PlaybackImageButton::UpdatePosition() {
  CHECK(window_size_.has_value());

  SetPosition(gfx::Point((window_size_->width() / 2) - (size().width() / 2),
                         (window_size_->height() / 2) - (size().height() / 2)));
}

void PlaybackImageButton::SetPlayButtonBackground() {
  if (!base::FeatureList::IsEnabled(
          media::kVideoPictureInPictureControlsUpdate2024)) {
    return;
  }
  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysSecondaryContainer, kPlaybackButtonSize / 2));
}

void PlaybackImageButton::SetPauseButtonBackground() {
  if (!base::FeatureList::IsEnabled(
          media::kVideoPictureInPictureControlsUpdate2024)) {
    return;
  }

  SetBackground(views::CreateRoundedRectBackground(
      SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF), kPlaybackButtonSize / 2));
}

BEGIN_METADATA(PlaybackImageButton)
END_METADATA
