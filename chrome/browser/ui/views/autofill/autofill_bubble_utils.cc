// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_bubble_utils.h"

#include <memory>
#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace autofill {
namespace {

constexpr int kIconSize = 16;

}
std::unique_ptr<views::ImageButton> CreateEditButton(
    views::Button::PressedCallback callback) {
  std::unique_ptr<views::ImageButton> button =
      views::CreateVectorImageButtonWithNativeTheme(
          std::move(callback), vector_icons::kEditIcon, kIconSize);
  button->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_EDIT_BUTTON_TOOLTIP));
  InstallCircleHighlightPathGenerator(button.get());
  return button;
}
}  // namespace autofill
