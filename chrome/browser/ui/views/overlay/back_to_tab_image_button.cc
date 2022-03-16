// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/back_to_tab_image_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kBackToTabImageSize = 14;

}  // namespace

BackToTabImageButton::BackToTabImageButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          views::kLaunchIcon, kColorPipWindowForeground, kBackToTabImageSize));

  // Accessibility.
  const std::u16string back_to_tab_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT));
  SetAccessibleName(back_to_tab_button_label);
  SetTooltipText(back_to_tab_button_label);
}

BEGIN_METADATA(BackToTabImageButton, OverlayWindowImageButton)
END_METADATA
