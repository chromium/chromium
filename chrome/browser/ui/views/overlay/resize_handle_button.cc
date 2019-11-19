// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/resize_handle_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kResizeHandleButtonMargin = 4;
constexpr int kResizeHandleButtonSize = 16;

constexpr SkColor kResizeHandleIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

ResizeHandleButton::ResizeHandleButton(ButtonListener* listener)
    : ImageButton(listener) {
  SetSize(gfx::Size(kResizeHandleButtonSize, kResizeHandleButtonSize));
  SetImageForQuadrant(OverlayWindowViews::WindowQuadrant::kBottomRight);

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 resize_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_RESIZE_HANDLE_TEXT));
  SetAccessibleName(resize_button_label);
  SetTooltipText(resize_button_label);
  SetInstallFocusRingOnFocus(true);
}

ResizeHandleButton::~ResizeHandleButton() = default;

int ResizeHandleButton::GetHTComponent() const {
  if (!current_quadrant_)
    return HTNOWHERE;

  switch (current_quadrant_.value()) {
    case OverlayWindowViews::WindowQuadrant::kBottomLeft:
      return HTTOPRIGHT;
    case OverlayWindowViews::WindowQuadrant::kBottomRight:
      return HTTOPLEFT;
    case OverlayWindowViews::WindowQuadrant::kTopLeft:
      return HTBOTTOMRIGHT;
    case OverlayWindowViews::WindowQuadrant::kTopRight:
      return HTBOTTOMLEFT;
  }
}

void ResizeHandleButton::SetPosition(
    const gfx::Size& size,
    OverlayWindowViews::WindowQuadrant quadrant) {
  // The resize handle should appear towards the center of the working area.
  // This is determined as the opposite quadrant on the window.
  switch (quadrant) {
    case OverlayWindowViews::WindowQuadrant::kBottomLeft:
      ImageButton::SetPosition(gfx::Point(
          size.width() - kResizeHandleButtonSize - kResizeHandleButtonMargin,
          kResizeHandleButtonMargin));
      break;
    case OverlayWindowViews::WindowQuadrant::kBottomRight:
      ImageButton::SetPosition(
          gfx::Point(kResizeHandleButtonMargin, kResizeHandleButtonMargin));
      break;
    case OverlayWindowViews::WindowQuadrant::kTopLeft:
      ImageButton::SetPosition(gfx::Point(
          size.width() - kResizeHandleButtonSize - kResizeHandleButtonMargin,
          size.height() - kResizeHandleButtonSize - kResizeHandleButtonMargin));
      break;
    case OverlayWindowViews::WindowQuadrant::kTopRight:
      ImageButton::SetPosition(gfx::Point(
          kResizeHandleButtonMargin,
          size.height() - kResizeHandleButtonSize - kResizeHandleButtonMargin));
      break;
  }

  // Also rotate the icon to match the new corner.
  SetImageForQuadrant(quadrant);
}

void ResizeHandleButton::SetImageForQuadrant(
    OverlayWindowViews::WindowQuadrant quadrant) {
  if (current_quadrant_ == quadrant)
    return;
  current_quadrant_ = quadrant;

  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      kResizeHandleIcon, kResizeHandleButtonSize, kResizeHandleIconColor);
  switch (quadrant) {
    case OverlayWindowViews::WindowQuadrant::kBottomLeft:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_TOP);
      break;
    case OverlayWindowViews::WindowQuadrant::kBottomRight:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_LEFT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_TOP);
      icon = gfx::ImageSkiaOperations::CreateRotatedImage(
          icon, SkBitmapOperations::ROTATION_270_CW);
      break;
    case OverlayWindowViews::WindowQuadrant::kTopLeft:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
      icon = gfx::ImageSkiaOperations::CreateRotatedImage(
          icon, SkBitmapOperations::ROTATION_90_CW);
      break;
    case OverlayWindowViews::WindowQuadrant::kTopRight:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_LEFT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
      icon = gfx::ImageSkiaOperations::CreateRotatedImage(
          icon, SkBitmapOperations::ROTATION_180_CW);
      break;
  }

  SetImage(views::Button::STATE_NORMAL, icon);
}

}  // namespace views
