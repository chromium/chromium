// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/toggle_microphone_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

ToggleMicrophoneButton::ToggleMicrophoneButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
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

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, width(), kPipWindowIconColor));
  SetTooltipText(l10n_util::GetStringUTF16(text));
}
