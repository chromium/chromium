// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_button_view.h"

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

ReadAnythingButtonView::ReadAnythingButtonView(
    views::ImageButton::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int icon_size,
    SkColor icon_color,
    const std::u16string& tooltip)
    : ImageButton(std::move(callback)) {
  views::SetImageFromVectorIconWithColorId(this, icon, icon_color, icon_color,
                                           icon_size);
  ConfigureInkDropForToolbar(this);
  views::InstallCircleHighlightPathGenerator(this);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kInternalInsets / 2, kInternalInsets / 2)));
  SetTooltipText(tooltip);
}

bool ReadAnythingButtonView::IsGroupFocusTraversable() const {
  // Only the first item in the toolbar should be reachable with tab
  return false;
}

void ReadAnythingButtonView::UpdateIcon(const gfx::VectorIcon& icon,
                                        int icon_size,
                                        ui::ColorId icon_color) {
  views::SetImageFromVectorIconWithColorId(this, icon, icon_color, icon_color,
                                           icon_size);
}

void ReadAnythingButtonView::Enable() {
  SetState(views::Button::ButtonState::STATE_NORMAL);
}

void ReadAnythingButtonView::Disable() {
  SetState(views::Button::ButtonState::STATE_DISABLED);
}

BEGIN_METADATA(ReadAnythingButtonView, views::View)
END_METADATA

ReadAnythingButtonView::~ReadAnythingButtonView() = default;
