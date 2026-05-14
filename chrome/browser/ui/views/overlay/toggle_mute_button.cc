// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/toggle_mute_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"

ToggleMuteButton::ToggleMuteButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  UpdateImageAndTooltipText();
}

void ToggleMuteButton::SetMutedState(bool is_muted) {
  is_muted_ = is_muted;
  UpdateImageAndTooltipText();
}

void ToggleMuteButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size()) {
    return;
  }

  UpdateImageAndTooltipText();
}

void ToggleMuteButton::UpdateImageAndTooltipText() {
  if (bounds().IsEmpty()) {
    return;
  }

  const auto& icon = is_muted_ ? vector_icons::kVolumeOffChromeRefreshOldIcon
                               : vector_icons::kVolumeUpChromeRefreshOldIcon;

  auto text = is_muted_ ? IDS_PICTURE_IN_PICTURE_UNMUTE_TEXT
                        : IDS_PICTURE_IN_PICTURE_MUTE_TEXT;

  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, kColorPipWindowForeground,
                                               icon_size));
  SetTooltipText(l10n_util::GetStringUTF16(text));
}

BEGIN_METADATA(ToggleMuteButton)
END_METADATA
