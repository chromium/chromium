// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/md_text_button_with_down_arrow.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"

namespace views {

MdTextButtonWithDownArrow::MdTextButtonWithDownArrow(PressedCallback callback,
                                                     const std::u16string& text)
    : MdTextButton(std::move(callback), text) {
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetImageLabelSpacing(LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING));
  SetDropArrowImage();

  // Reduce padding between the drop arrow and the right border.
  const gfx::Insets original_padding = GetInsets();
  SetBorder(CreateEmptyBorder(
      gfx::Insets::TLBR(original_padding.top(), original_padding.left(),
                        original_padding.bottom(),
                        LayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN))));
}

MdTextButtonWithDownArrow::~MdTextButtonWithDownArrow() = default;

void MdTextButtonWithDownArrow::OnThemeChanged() {
  MdTextButton::OnThemeChanged();

  // The icon's color is derived from the label's |enabled_color|, which might
  // have changed as the result of the theme change.
  SetDropArrowImage();
}

void MdTextButtonWithDownArrow::SetDropArrowImage() {
  auto drop_arrow_image = ui::ImageModel::FromVectorIcon(
      kMenuDropArrowIcon,
      color_utils::DeriveDefaultIconColor(label()->GetEnabledColor()));
  SetImageModel(Button::STATE_NORMAL, drop_arrow_image);
}

BEGIN_METADATA(MdTextButtonWithDownArrow, views::MdTextButton)
END_METADATA

}  // namespace views
