// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/md_text_button_with_down_arrow.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"

namespace views {

MdTextButtonWithDownArrow::MdTextButtonWithDownArrow(ButtonListener* listener,
                                                     const base::string16& text)
    : MdTextButton(listener, style::CONTEXT_BUTTON_MD) {
  SetText(text);
  SetFocusForPlatform();
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  SetImageLabelSpacing(LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING));
  SetDropArrowImage();

  // Reduce padding between the drop arrow and the right border.
  const gfx::Insets original_padding = border()->GetInsets();
  SetBorder(CreateEmptyBorder(original_padding.top(), original_padding.left(),
                              original_padding.bottom(),
                              LayoutProvider::Get()->GetDistanceMetric(
                                  DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN)));
}

MdTextButtonWithDownArrow::~MdTextButtonWithDownArrow() = default;

void MdTextButtonWithDownArrow::OnThemeChanged() {
  MdTextButton::OnThemeChanged();

  // The icon's color is derived from the label's |enabled_color|, which might
  // have changed as the result of the theme change.
  SetDropArrowImage();
}

void MdTextButtonWithDownArrow::SetDropArrowImage() {
  gfx::ImageSkia drop_arrow_image = gfx::CreateVectorIcon(
      kMenuDropArrowIcon,
      color_utils::DeriveDefaultIconColor(label()->GetEnabledColor()));
  SetImage(Button::STATE_NORMAL, drop_arrow_image);
}

}  // namespace views
