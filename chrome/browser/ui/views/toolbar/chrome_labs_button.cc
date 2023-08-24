// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs_button.h"

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs_coordinator.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/dot_indicator.h"
#include "ui/views/view_class_properties.h"

ChromeLabsButton::ChromeLabsButton(BrowserView* browser_view,
                                   const ChromeLabsModel* model)
    : ToolbarButton(base::BindRepeating(&ChromeLabsButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_view_(browser_view),
      model_(model) {
  SetProperty(views::kElementIdentifierKey, kToolbarChromeLabsButtonElementId);
  SetVectorIcons(features::IsChromeRefresh2023() ? kChromeLabsChromeRefreshIcon
                                                 : kChromeLabsIcon,
                 kChromeLabsTouchIcon);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CHROMELABS_BUTTON));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_CHROMELABS_BUTTON));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kPopUpButton);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);
  new_experiments_indicator_ = views::DotIndicator::Install(image());
  UpdateDotIndicator();

  chrome_labs_coordinator_ = std::make_unique<ChromeLabsCoordinator>(
      this, browser_view_->browser(), model_);
}

ChromeLabsButton::~ChromeLabsButton() = default;

void ChromeLabsButton::Layout() {
  ToolbarButton::Layout();
  gfx::Rect dot_rect(8, 8);
  if (ui::TouchUiController::Get()->touch_ui()) {
    dot_rect = ScaleToEnclosingRect(
        dot_rect, float{kDefaultTouchableIconSize} / kDefaultIconSize);
  }
  dot_rect.set_origin(image()->GetImageBounds().bottom_right() -
                      dot_rect.bottom_right().OffsetFromOrigin());
  new_experiments_indicator_->SetBoundsRect(dot_rect);
}

void ChromeLabsButton::HideDotIndicator() {
  new_experiments_indicator_->Hide();
}

void ChromeLabsButton::ButtonPressed() {
  chrome_labs_coordinator_->ShowOrHide();
}

void ChromeLabsButton::UpdateDotIndicator() {
  bool should_show_dot_indicator = AreNewChromeLabsExperimentsAvailable(
      model_, browser_view_->browser()->profile());

  if (should_show_dot_indicator)
    new_experiments_indicator_->Show();
  else
    new_experiments_indicator_->Hide();
}

BEGIN_METADATA(ChromeLabsButton, ToolbarButton)
END_METADATA
