// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_button_view.h"

#include <utility>

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"

ReadAnythingButtonView::ReadAnythingButtonView(
    views::ImageButton::PressedCallback callback,
    const std::u16string& tooltip)
    : ImageButton(std::move(callback)) {
  ConfigureInkDropForToolbar(this);
  views::InstallCircleHighlightPathGenerator(this);
  SetTooltipText(tooltip);
}

bool ReadAnythingButtonView::IsGroupFocusTraversable() const {
  // Only the first item in the toolbar should be reachable with tab.
  return false;
}

void ReadAnythingButtonView::UpdateIcon(const gfx::VectorIcon& icon,
                                        int icon_size,
                                        ui::ColorId icon_color,
                                        ui::ColorId focus_ring_color) {
  views::SetImageFromVectorIconWithColorId(this, icon, icon_color, icon_color,
                                           icon_size);
  DCHECK(views::FocusRing::Get(this));
  views::FocusRing::Get(this)->SetColorId(focus_ring_color);
}

void ReadAnythingButtonView::Enable() {
  SetState(views::Button::ButtonState::STATE_NORMAL);
}

void ReadAnythingButtonView::Disable() {
  SetState(views::Button::ButtonState::STATE_DISABLED);
}

BEGIN_METADATA(ReadAnythingButtonView)
END_METADATA

ReadAnythingButtonView::~ReadAnythingButtonView() = default;
