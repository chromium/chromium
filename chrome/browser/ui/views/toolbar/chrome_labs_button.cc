// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/metadata/metadata_impl_macros.h"

ChromeLabsButton::ChromeLabsButton(Browser* browser,
                                   const ChromeLabsBubbleViewModel* model)
    : ToolbarButton(base::BindRepeating(&ChromeLabsButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser),
      model_(model) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CHROMELABS_BUTTON));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_CHROMELABS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kPopUpButton);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);
}

ChromeLabsButton::~ChromeLabsButton() {
  // Make sure the bubble is destroyed if the button is being destroyed.
  ChromeLabsBubbleView::Hide();
}

void ChromeLabsButton::UpdateIcon() {
  const gfx::VectorIcon& chrome_labs_image =
      ui::TouchUiController::Get()->touch_ui() ? kChromeLabsTouchIcon
                                               : kChromeLabsIcon;
  UpdateIconsWithStandardColors(chrome_labs_image);
}

void ChromeLabsButton::ButtonPressed() {
  if (ChromeLabsBubbleView::IsShowing()) {
    ChromeLabsBubbleView::Hide();
    return;
  }
  ChromeLabsBubbleView::Show(this, browser_, model_);
}

// static
bool ChromeLabsButton::ShouldShowButton(
    const ChromeLabsBubbleViewModel* model) {
  const std::vector<LabInfo>& all_labs = model->GetLabInfo();
  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        about_flags::GetCurrentFlagsState()->FindFeatureEntryByName(
            lab.internal_name);
    if ((entry && (entry->supported_platforms &
                   flags_ui::FlagsState::GetCurrentPlatform()) != 0) &&
        chrome::GetChannel() <= lab.allowed_channel) {
      return true;
    }
  }
  return false;
}

BEGIN_METADATA(ChromeLabsButton, ToolbarButton)
END_METADATA
