// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"

SidePanelToolbarButton::SidePanelToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&SidePanelToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser) {
  pref_change_registrar_.Init(browser_->profile()->GetPrefs());

  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&SidePanelToolbarButton::UpdateToolbarButtonIcon,
                          base::Unretained(this)));

  UpdateToolbarButtonIcon();
  SetTooltipText(l10n_util::GetStringUTF16(
      companion::IsCompanionFeatureEnabled() ? IDS_TOOLTIP_SIDE_PANEL
                                             : IDS_TOOLTIP_SIDE_PANEL_SHOW));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kMenu);
  SetProperty(views::kElementIdentifierKey, kToolbarSidePanelButtonElementId);
}

SidePanelToolbarButton::~SidePanelToolbarButton() = default;

void SidePanelToolbarButton::ButtonPressed() {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_);
  DCHECK(browser_view->unified_side_panel());
  SidePanelUI::GetSidePanelUIForBrowser(browser_)->Toggle();
}

void SidePanelToolbarButton::UpdateToolbarButtonIcon() {
  const bool is_right_aligned = browser_->profile()->GetPrefs()->GetBoolean(
      prefs::kSidePanelHorizontalAlignment);
  if (is_right_aligned)
    SetVectorIcons(features::IsChromeRefresh2023() ? kSidePanelChromeRefreshIcon
                                                   : kSidePanelIcon,
                   kSidePanelTouchIcon);
  else
    SetVectorIcons(features::IsChromeRefresh2023()
                       ? kSidePanelLeftChromeRefreshIcon
                       : kSidePanelLeftIcon,
                   kSidePanelLeftTouchIcon);
}

bool SidePanelToolbarButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}

BEGIN_METADATA(SidePanelToolbarButton, ToolbarButton)
END_METADATA
