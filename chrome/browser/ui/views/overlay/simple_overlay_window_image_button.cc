// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/simple_overlay_window_image_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/overlay/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"

SimpleOverlayWindowImageButton::SimpleOverlayWindowImageButton(
    PressedCallback callback,
    const gfx::VectorIcon& icon,
    std::u16string label)
    : OverlayWindowImageButton(std::move(callback)), icon_(icon) {
  UpdateImage();

  // Accessibility.
  GetViewAccessibility().SetName(label);
  SetTooltipText(label);
}

void SimpleOverlayWindowImageButton::SetVisible(bool visible) {
  // We need to do more than the usual visibility change because otherwise the
  // overlay window cannot be dragged when grabbing within the button area.
  views::ImageButton::SetVisible(visible);
  SetSize(visible ? last_visible_size_ : gfx::Size());
}

void SimpleOverlayWindowImageButton::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (!size().IsEmpty())
    last_visible_size_ = size();

  if (size() == previous_bounds.size())
    return;

  UpdateImage();
}

void SimpleOverlayWindowImageButton::UpdateImage() {
  const int icon_size = std::max(0, width() - (2 * kPipWindowIconPadding));
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    *icon_, kColorPipWindowForeground, icon_size));
}

BEGIN_METADATA(SimpleOverlayWindowImageButton)
END_METADATA
