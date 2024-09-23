// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/back_to_tab_button.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace {

constexpr int kBackToTabButtonVerticalMargin = 4;
constexpr int kBackToTabButtonHorizontalMargin = 32;
constexpr int kBackToTabButtonSize = 24;
constexpr int kBackToTabButtonIconSize = 16;

}  // namespace

OverlayWindowBackToTabButton::OverlayWindowBackToTabButton(
    PressedCallback callback)
    : OverlayWindowImageButton(std::move(callback)) {
  SetSize(gfx::Size(kBackToTabButtonSize, kBackToTabButtonSize));

  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    vector_icons::kBackToTabChromeRefreshIcon,
                    kColorPipWindowForeground, kBackToTabButtonIconSize));

  // Accessibility.
  const std::u16string button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT));
  GetViewAccessibility().SetName(button_label);
  SetTooltipText(button_label);
}

void OverlayWindowBackToTabButton::SetPosition(
    const gfx::Size& size,
    VideoOverlayWindowViews::WindowQuadrant quadrant) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (quadrant == VideoOverlayWindowViews::WindowQuadrant::kBottomLeft) {
    views::ImageButton::SetPosition(gfx::Point(kBackToTabButtonHorizontalMargin,
                                               kBackToTabButtonVerticalMargin));
    return;
  }
#endif

  views::ImageButton::SetPosition(gfx::Point(
      size.width() - kBackToTabButtonSize - kBackToTabButtonHorizontalMargin,
      kBackToTabButtonVerticalMargin));
}

BEGIN_METADATA(OverlayWindowBackToTabButton)
END_METADATA
