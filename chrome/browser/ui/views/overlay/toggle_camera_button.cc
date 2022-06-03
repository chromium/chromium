// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/toggle_camera_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

ToggleCameraButton::ToggleCameraButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
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

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, width(), kPipWindowIconColor));
  SetTooltipText(l10n_util::GetStringUTF16(text));
}
