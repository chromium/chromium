// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/metadata/metadata_impl_macros.h"

ChromeLabsButton::ChromeLabsButton()
    : ToolbarButton(base::BindRepeating(&ChromeLabsButton::ButtonPressed,
                                        base::Unretained(this))) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_TOOLTIP_CHROMELABS_BUTTON));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_CHROMELABS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
}

ChromeLabsButton::~ChromeLabsButton() = default;

void ChromeLabsButton::UpdateIcon() {
  const gfx::VectorIcon& chrome_labs_image =
      ui::TouchUiController::Get()->touch_ui() ? kChromeLabsTouchIcon
                                               : kChromeLabsIcon;
  UpdateIconsWithStandardColors(chrome_labs_image);
}

void ChromeLabsButton::SetLabInfoForTesting(
    const std::vector<LabInfo>& test_lab_info) {
  test_lab_info_ = test_lab_info;
}

void ChromeLabsButton::ButtonPressed() {
  if (ChromeLabsBubbleView::IsShowing()) {
    ChromeLabsBubbleView::Hide();
    return;
  }
  std::unique_ptr<ChromeLabsBubbleViewModel> model =
      std::make_unique<ChromeLabsBubbleViewModel>(test_lab_info_);
  ChromeLabsBubbleView::Show(this, std::move(model));
}

BEGIN_METADATA(ChromeLabsButton, ToolbarButton)
END_METADATA
