// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/minimize_button.h"

#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

constexpr int kMinimizeButtonVerticalMargin = 4;
constexpr int kMinimizeButtonHorizontalMargin = 60;
constexpr int kMinimizeButtonSize = 24;
constexpr int kMinimizeButtonIconSize = 16;

}  // namespace

OverlayWindowMinimizeButton::OverlayWindowMinimizeButton(
    PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetSize(gfx::Size(kMinimizeButtonSize, kMinimizeButtonSize));

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(kChromiumMinimizeIcon,
                                               kColorPipWindowForeground,
                                               kMinimizeButtonIconSize));

  // Accessibility.
  const std::u16string button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_MINIMIZE_CONTROL_TEXT));
  GetViewAccessibility().SetName(button_label);
  SetTooltipText(button_label);
}

void OverlayWindowMinimizeButton::SetPosition(
    const gfx::Size& size,
    VideoOverlayWindowViews::WindowQuadrant quadrant) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (quadrant == VideoOverlayWindowViews::WindowQuadrant::kBottomLeft) {
    views::ImageButton::SetPosition(gfx::Point(kMinimizeButtonHorizontalMargin,
                                               kMinimizeButtonVerticalMargin));
    return;
  }
#endif

  views::ImageButton::SetPosition(gfx::Point(
      size.width() - kMinimizeButtonSize - kMinimizeButtonHorizontalMargin,
      kMinimizeButtonVerticalMargin));
}

BEGIN_METADATA(OverlayWindowMinimizeButton)
END_METADATA
