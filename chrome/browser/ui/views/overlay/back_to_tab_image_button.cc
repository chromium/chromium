// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/back_to_tab_image_button.h"

#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"

namespace {

const int kBackToTabImageSize = 14;

constexpr SkColor kBackToTabIconColor = SK_ColorWHITE;

}  // namespace

namespace views {

BackToTabImageButton::BackToTabImageButton(ButtonListener* listener)
    : ImageButton(listener) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(views::kLaunchIcon, kBackToTabImageSize,
                                 kBackToTabIconColor));

  // Accessibility.
  SetFocusForPlatform();
  const base::string16 back_to_tab_button_label(l10n_util::GetStringUTF16(
      IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT));
  SetAccessibleName(back_to_tab_button_label);
  SetTooltipText(back_to_tab_button_label);
  SetInstallFocusRingOnFocus(true);
}

}  // namespace views
