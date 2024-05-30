// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/close_image_button.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kCloseButtonMargin = 4;
constexpr int kCloseButtonSize = 24;
constexpr int kCloseButtonIconSize = 16;

}  // namespace

CloseImageButton::CloseImageButton(PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetSize(gfx::Size(kCloseButtonSize, kCloseButtonSize));

  auto* icon = &vector_icons::kCloseChromeRefreshIcon;
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon, kColorPipWindowForeground,
                                               kCloseButtonIconSize));

  // Accessibility.
  const std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT));
  GetViewAccessibility().SetName(close_button_label);
  SetTooltipText(close_button_label);
}

void CloseImageButton::SetPosition(
    const gfx::Size& size,
    VideoOverlayWindowViews::WindowQuadrant quadrant) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (quadrant == VideoOverlayWindowViews::WindowQuadrant::kBottomLeft) {
    views::ImageButton::SetPosition(
        gfx::Point(kCloseButtonMargin, kCloseButtonMargin));
    return;
  }
#endif

  views::ImageButton::SetPosition(
      gfx::Point(size.width() - kCloseButtonSize - kCloseButtonMargin,
                 kCloseButtonMargin));
}

BEGIN_METADATA(CloseImageButton)
END_METADATA
