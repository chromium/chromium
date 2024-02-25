// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/toggle_microphone_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"

ToggleMicrophoneButton::ToggleMicrophoneButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  UpdateImageAndTooltipText();
}

void ToggleMicrophoneButton::SetMutedState(bool is_muted) {
  is_muted_ = is_muted;
  UpdateImageAndTooltipText();
}

void ToggleMicrophoneButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size())
    return;

  UpdateImageAndTooltipText();
}

void ToggleMicrophoneButton::UpdateImageAndTooltipText() {
  if (bounds().IsEmpty())
    return;

  const auto& icon =
      is_muted_ ? vector_icons::kMicOffIcon : vector_icons::kMicIcon;
  auto text = is_muted_ ? IDS_PICTURE_IN_PICTURE_UNMUTE_MICROPHONE_TEXT
                        : IDS_PICTURE_IN_PICTURE_MUTE_MICROPHONE_TEXT;
  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, kColorPipWindowForeground,
                                               icon_size));
  SetTooltipText(l10n_util::GetStringUTF16(text));
}

BEGIN_METADATA(ToggleMicrophoneButton)
END_METADATA
