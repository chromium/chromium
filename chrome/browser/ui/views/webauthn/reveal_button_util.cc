// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/reveal_button_util.h"

#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/vector_icons.h"

namespace {
constexpr int kEyePaddingWidth = 4;
}  // namespace

std::unique_ptr<views::ToggleImageButton> CreateRevealButton(
    views::ImageButton::PressedCallback callback) {
  auto button =
      views::Builder<views::ToggleImageButton>()
          .SetInstallFocusRingOnFocus(true)
          .SetRequestFocusOnPress(true)
          .SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE)
          .SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER)
          .SetCallback(std::move(callback))
          .SetBorder(views::CreateEmptyBorder(kEyePaddingWidth))
          .SetTooltipText(u"Tooltip (UNTRANSLATED)")
          .SetToggledTooltipText(u"Toggled tooltip (UNTRANSLATED)")
          .Build();
  SetImageFromVectorIconWithColorId(button.get(), views::kEyeIcon,
                                    ui::kColorIcon, ui::kColorIconDisabled);
  SetToggledImageFromVectorIconWithColorId(button.get(), views::kEyeCrossedIcon,
                                           ui::kColorIcon,
                                           ui::kColorIconDisabled);
  return button;
}
