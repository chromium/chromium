// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/toggle_camera_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"

ToggleCameraButton::ToggleCameraButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  UpdateImageAndTooltipText();
}

void ToggleCameraButton::SetCameraState(bool is_turned_on) {
  is_turned_on_ = is_turned_on;
  UpdateImageAndTooltipText();
}

void ToggleCameraButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size())
    return;

  UpdateImageAndTooltipText();
}

void ToggleCameraButton::UpdateImageAndTooltipText() {
  if (bounds().IsEmpty())
    return;

  const auto& icon = is_turned_on_ ? vector_icons::kVideocamIcon
                                   : vector_icons::kVideocamOffIcon;
  auto text = is_turned_on_ ? IDS_PICTURE_IN_PICTURE_TURN_OFF_CAMERA_TEXT
                            : IDS_PICTURE_IN_PICTURE_TURN_ON_CAMERA_TEXT;
  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(icon, kColorPipWindowForeground,
                                               icon_size));
  SetTooltipText(l10n_util::GetStringUTF16(text));
}

BEGIN_METADATA(ToggleCameraButton)
END_METADATA
