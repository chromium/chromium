// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/reveal_button_util.h"

#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"

namespace {
constexpr int kEyeIconSize = 20;
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
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_WEBAUTHN_SHOW_PIN))
          .SetToggledTooltipText(
              l10n_util::GetStringUTF16(IDS_WEBAUTHN_HIDE_PIN))
          .Build();
  SetImageFromVectorIconWithColorId(button.get(), vector_icons::kVisibilityIcon,
                                    ui::kColorIcon, ui::kColorIconDisabled,
                                    kEyeIconSize);
  SetToggledImageFromVectorIconWithColorId(
      button.get(), vector_icons::kVisibilityOffIcon, ui::kColorIcon,
      ui::kColorIconDisabled, kEyeIconSize);
  views::InkDrop::Get(button.get())
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  button->SetShowInkDropWhenHotTracked(true);
  return button;
}
