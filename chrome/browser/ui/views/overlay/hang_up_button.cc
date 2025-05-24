// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/hang_up_button.h"

#include "base/feature_list.h"
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
#include "ui/views/background.h"

HangUpButton::HangUpButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_HANG_UP_TEXT));
  UpdateImage();
}

void HangUpButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size()) {
    return;
  }

  UpdateImage();
}

void HangUpButton::UpdateImage() {
  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));

  ui::ColorId icon_color = kColorPipWindowHangUpButtonForeground;
  if (base::FeatureList::IsEnabled(
          media::kVideoPictureInPictureControlsUpdate2024)) {
    icon_color = ui::kColorSysOnError;
    SetBackground(
        views::CreateRoundedRectBackground(ui::kColorSysError, width() / 2));
  }

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(vector_icons::kCallEndIcon,
                                               icon_color, icon_size));
}

BEGIN_METADATA(HangUpButton)
END_METADATA
