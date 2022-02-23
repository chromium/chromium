// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"

#include "chrome/browser/ui/views/overlay/constants.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/native_cursor.h"

OverlayWindowImageButton::OverlayWindowImageButton(PressedCallback callback)
    : ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  views::ConfigureVectorImageButton(this);
  views::InkDrop::Get(this)->SetBaseColor(kPipWindowIconColor);

  SetInstallFocusRingOnFocus(true);
}

gfx::NativeCursor OverlayWindowImageButton::GetCursor(
    const ui::MouseEvent& event) {
  return views::GetNativeHandCursor();
}

BEGIN_METADATA(OverlayWindowImageButton, views::ImageButton)
END_METADATA
