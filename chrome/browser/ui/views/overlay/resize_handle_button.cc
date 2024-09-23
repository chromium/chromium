// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/resize_handle_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kResizeHandleButtonMargin = 4;
constexpr int kResizeHandleButtonSize = 16;

}  // namespace

ResizeHandleButton::ResizeHandleButton(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  SetSize(gfx::Size(kResizeHandleButtonSize, kResizeHandleButtonSize));

  // The ResizeHandleButton has no action and is just for display to hint to the
  // user that the window can be dragged. It should not be focusable.
  SetFocusBehavior(FocusBehavior::NEVER);

  // Accessibility.
  const std::u16string resize_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_RESIZE_HANDLE_TEXT));
  GetViewAccessibility().SetName(resize_button_label);
  SetTooltipText(resize_button_label);
}

ResizeHandleButton::~ResizeHandleButton() = default;

void ResizeHandleButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  UpdateImageForQuadrant();
}

int ResizeHandleButton::GetHTComponent() const {
  switch (current_quadrant_) {
    case VideoOverlayWindowViews::WindowQuadrant::kBottomLeft:
      return HTTOPRIGHT;
    case VideoOverlayWindowViews::WindowQuadrant::kBottomRight:
      return HTTOPLEFT;
    case VideoOverlayWindowViews::WindowQuadrant::kTopLeft:
      return HTBOTTOMRIGHT;
    case VideoOverlayWindowViews::WindowQuadrant::kTopRight:
      return HTBOTTOMLEFT;
  }
}

void ResizeHandleButton::SetPosition(
    const gfx::Size& size,
    VideoOverlayWindowViews::WindowQuadrant quadrant) {
  // The resize handle should appear towards the center of the working area.
  // This is determined as the opposite quadrant on the window.
  switch (quadrant) {
    case VideoOverlayWindowViews::WindowQuadrant::kBottomLeft:
      views::ImageButton::SetPosition(gfx::Point(
          size.width() - kResizeHandleButtonSize - kResizeHandleButtonMargin,
          kResizeHandleButtonMargin));
      break;
    case VideoOverlayWindowViews::WindowQuadrant::kBottomRight:
      views::ImageButton::SetPosition(
          gfx::Point(kResizeHandleButtonMargin, kResizeHandleButtonMargin));
      break;
    case VideoOverlayWindowViews::WindowQuadrant::kTopLeft:
      views::ImageButton::SetPosition(gfx::Point(
          size.width() - kResizeHandleButtonSize - kResizeHandleButtonMargin,
          size.height() - kResizeHandleButtonSize - kResizeHandleButtonMargin));
      break;
    case VideoOverlayWindowViews::WindowQuadrant::kTopRight:
      views::ImageButton::SetPosition(gfx::Point(
          kResizeHandleButtonMargin,
          size.height() - kResizeHandleButtonSize - kResizeHandleButtonMargin));
      break;
  }

  // Also rotate the icon to match the new corner.
  SetQuadrant(quadrant);
}

void ResizeHandleButton::SetQuadrant(
    VideoOverlayWindowViews::WindowQuadrant quadrant) {
  if (current_quadrant_ == quadrant)
    return;
  current_quadrant_ = quadrant;
  if (GetWidget())
    UpdateImageForQuadrant();
}

void ResizeHandleButton::UpdateImageForQuadrant() {
  const SkColor color = GetColorProvider()->GetColor(kColorPipWindowForeground);
  gfx::ImageSkia icon =
      gfx::CreateVectorIcon(kResizeHandleIcon, kResizeHandleButtonSize, color);
  switch (current_quadrant_) {
    case VideoOverlayWindowViews::WindowQuadrant::kBottomLeft:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_TOP);
      break;
    case VideoOverlayWindowViews::WindowQuadrant::kBottomRight:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_LEFT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_TOP);
      icon = gfx::ImageSkiaOperations::CreateRotatedImage(
          icon, SkBitmapOperations::ROTATION_270_CW);
      break;
    case VideoOverlayWindowViews::WindowQuadrant::kTopLeft:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
      icon = gfx::ImageSkiaOperations::CreateRotatedImage(
          icon, SkBitmapOperations::ROTATION_90_CW);
      break;
    case VideoOverlayWindowViews::WindowQuadrant::kTopRight:
      SetImageHorizontalAlignment(views::ImageButton::ALIGN_LEFT);
      SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
      icon = gfx::ImageSkiaOperations::CreateRotatedImage(
          icon, SkBitmapOperations::ROTATION_180_CW);
      break;
  }

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromImageSkia(icon));
}

BEGIN_METADATA(ResizeHandleButton)
END_METADATA
