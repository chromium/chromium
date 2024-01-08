// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"

OverlayWindowImageButton::OverlayWindowImageButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  views::ConfigureVectorImageButton(this);
  views::InstallCircleHighlightPathGenerator(this);

  SetInstallFocusRingOnFocus(true);
}

ui::Cursor OverlayWindowImageButton::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kHand;
}

void OverlayWindowImageButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();

  views::InkDrop::Get(this)->SetBaseColor(
      GetColorProvider()->GetColor(kColorPipWindowForeground));
}

BEGIN_METADATA(OverlayWindowImageButton)
END_METADATA
