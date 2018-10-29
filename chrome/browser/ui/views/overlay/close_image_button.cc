// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/close_image_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kCloseButtonMargin = 8;
const int kCloseButtonSize = 24;

constexpr SkColor kCloseBgColor = gfx::kGoogleGrey700;
constexpr SkColor kCloseIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

CloseImageButton::CloseImageButton(ButtonListener* listener)
    : ImageButton(listener),
      close_background_(
          gfx::CreateVectorIcon(kPictureInPictureControlBackgroundIcon,
                                kCloseButtonSize,
                                kCloseBgColor)) {
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetBackgroundImageAlignment(views::ImageButton::ALIGN_LEFT,
                              views::ImageButton::ALIGN_TOP);
  SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(views::kIcCloseIcon,
                                 std::round(kCloseButtonSize * 2.0 / 3.0),
                                 kCloseIconColor));

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 close_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT));
  SetAccessibleName(close_button_label);
  SetTooltipText(close_button_label);
  SetInstallFocusRingOnFocus(true);
}

void CloseImageButton::StateChanged(ButtonState old_state) {
  ImageButton::StateChanged(old_state);

  if (state() == STATE_HOVERED || state() == STATE_PRESSED)
    SetBackgroundImage(kCloseBgColor, &close_background_, &close_background_);
  else
    SetBackgroundImage(kCloseBgColor, nullptr, nullptr);
}

void CloseImageButton::OnFocus() {
  ImageButton::OnFocus();
  SetBackgroundImage(kCloseBgColor, &close_background_, &close_background_);
}

void CloseImageButton::OnBlur() {
  ImageButton::OnBlur();
  SetBackgroundImage(kCloseBgColor, nullptr, nullptr);
}

void CloseImageButton::SetPosition(
    const gfx::Size& size,
    OverlayWindowViews::WindowQuadrant quadrant) {
#if defined(OS_CHROMEOS)
  if (quadrant == OverlayWindowViews::WindowQuadrant::kBottomLeft) {
    ImageButton::SetPosition(
        gfx::Point(kCloseButtonMargin, kCloseButtonMargin));
    return;
  }
#endif

  ImageButton::SetPosition(
      gfx::Point(size.width() - kCloseButtonSize - kCloseButtonMargin,
                 kCloseButtonMargin));
}

}  // namespace views
