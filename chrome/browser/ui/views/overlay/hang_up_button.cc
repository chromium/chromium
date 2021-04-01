// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/hang_up_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

HangUpButton::HangUpButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_HANG_UP_TEXT));
  UpdateImage();
}

void HangUpButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size())
    return;

  UpdateImage();
}

void HangUpButton::UpdateImage() {
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(vector_icons::kCallEndIcon, width(),
                                 kPipWindowIconColor));
}
