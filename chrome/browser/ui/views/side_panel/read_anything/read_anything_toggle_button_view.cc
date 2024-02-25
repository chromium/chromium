// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toggle_button_view.h"

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"

ReadAnythingToggleButtonView::ReadAnythingToggleButtonView(
    bool initial_toggled_state,
    views::ImageButton::PressedCallback callback,
    const std::u16string& toggled_tooltip,
    const std::u16string& untoggled_tooltip)
    : views::ToggleImageButton(std::move(callback)) {
  ConfigureInkDropForToolbar(this);
  views::InstallCircleHighlightPathGenerator(this);
  SetTooltipText(untoggled_tooltip);
  SetToggledTooltipText(toggled_tooltip);

  SetToggled(initial_toggled_state);
}

ReadAnythingToggleButtonView::~ReadAnythingToggleButtonView() = default;

void ReadAnythingToggleButtonView::UpdateIcons(
    const gfx::VectorIcon& toggled_icon,
    const gfx::VectorIcon& untoggled_icon,
    int icon_size,
    ui::ColorId icon_color,
    ui::ColorId focus_ring_color) {
  views::SetImageFromVectorIconWithColorId(this, untoggled_icon, icon_color,
                                           icon_color, icon_size);
  views::SetToggledImageFromVectorIconWithColorId(
      this, toggled_icon, icon_color, icon_color, icon_size);
  auto* focus_ring = views::FocusRing::Get(this);
  CHECK(focus_ring);
  focus_ring->SetColorId(focus_ring_color);
}

bool ReadAnythingToggleButtonView::IsGroupFocusTraversable() const {
  // Only the first item in the toolbar should be reachable with tab.
  return false;
}

BEGIN_METADATA(ReadAnythingToggleButtonView)
END_METADATA
