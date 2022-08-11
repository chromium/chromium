// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_side_panel_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

CustomizeChromeSidePanelController::CustomizeChromeSidePanelController() =
    default;

CustomizeChromeSidePanelController::~CustomizeChromeSidePanelController() =
    default;

void CustomizeChromeSidePanelController::CreateAndRegisterEntry(
    content::WebContents* web_contents) {
  auto* registry = SidePanelRegistry::Get(web_contents);

  if (!registry)
    return;

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kCustomizeChrome,
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE),
      // Icon needs to be changed to customize chrome icon.
      ui::ImageModel::FromVectorIcon(omnibox::kStarIcon, ui::kColorIcon),
      base::BindRepeating(
          &CustomizeChromeSidePanelController::CreateCustomizeChromeWebView,
          base::Unretained(this), web_contents));
  registry->Register(std::move(entry));
}

void CustomizeChromeSidePanelController::DeregisterEntry(
    content::WebContents* web_contents) {
  if (auto* registry = SidePanelRegistry::Get(web_contents))
    registry->Deregister(SidePanelEntry::Id::kCustomizeChrome);
}

std::unique_ptr<views::View>
CustomizeChromeSidePanelController::CreateCustomizeChromeWebView(
    content::WebContents* web_contents) {
  auto customize_chrome_web_view =
      std::make_unique<SidePanelWebUIViewT<CustomizeChromeUI>>(
          base::RepeatingClosure(), base::RepeatingClosure(),
          std::make_unique<BubbleContentsWrapperT<CustomizeChromeUI>>(
              GURL(chrome::kChromeUICustomizeChromeSidePanelURL),
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false));
  customize_chrome_web_view->ShowUI();
  return customize_chrome_web_view;
}
