// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/back_forward_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"

BackForwardButton::BackForwardButton(Direction direction,
                                     views::ButtonListener* listener,
                                     Browser* browser)
    : ToolbarButton(listener,
                    std::make_unique<BackForwardMenuModel>(
                        browser,
                        direction == Direction::kBack
                            ? BackForwardMenuModel::ModelType::kBackward
                            : BackForwardMenuModel::ModelType::kForward),
                    browser->tab_strip_model()),
      direction_(direction) {
  SetHideInkDropWhenShowingContextMenu(false);
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_MIDDLE_MOUSE_BUTTON);
  if (direction_ == Direction::kBack) {
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
    GetViewAccessibility().OverrideDescription(
        l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_BACK));
    SetID(VIEW_ID_BACK_BUTTON);
  } else {
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
    GetViewAccessibility().OverrideDescription(
        l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_FORWARD));
    SetID(VIEW_ID_FORWARD_BUTTON);
  }
}

BackForwardButton::~BackForwardButton() = default;

void BackForwardButton::UpdateIcon() {
  const gfx::VectorIcon* image = nullptr;
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  if (direction_ == Direction::kBack) {
    image = touch_ui ? &kBackArrowTouchIcon : &vector_icons::kBackArrowIcon;
  } else {
    image =
        touch_ui ? &kForwardArrowTouchIcon : &vector_icons::kForwardArrowIcon;
  }
  UpdateIconsWithStandardColors(*image);
}
