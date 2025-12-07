// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/hang_up_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"

HangUpButton::HangUpButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_HANG_UP_TEXT));
  UpdateImage();

  // We use a solid background color in the UI, and that ends up sitting above
  // the ink drop layer, so we need to force the ink drop layer higher here.
  views::InkDrop::Get(this)->SetLayerRegion(views::LayerRegion::kAbove);
}

void HangUpButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (size() == previous_bounds.size()) {
    return;
  }

  UpdateImage();
}

void HangUpButton::UpdateImage() {
  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));

  SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysError, width() / 2));

  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kCallEndIcon,
                                     ui::kColorSysOnError, icon_size));
}

BEGIN_METADATA(HangUpButton)
END_METADATA
