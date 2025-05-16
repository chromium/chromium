// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_ui.h"

#include <memory>

#include "ash/webui/common/trusted_types_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/floating_workspace_resources.h"
#include "chrome/grit/floating_workspace_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace ash {

FloatingWorkspaceUIConfig::FloatingWorkspaceUIConfig()
    : ChromeOSWebUIConfig(content::kChromeUIScheme,
                          chrome::kChromeUIFloatingWorkspaceDialogHost) {}

FloatingWorkspaceUI::FloatingWorkspaceUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIFloatingWorkspaceDialogHost);

  webui::SetupWebUIDataSource(source, kFloatingWorkspaceResources,
                              IDR_FLOATING_WORKSPACE_FLOATING_WORKSPACE_HTML);

  // Reuse animation from the OOBE consumer update screen.
  static constexpr webui::LocalizedString kAnimationMessage[] = {
      {"pauseAnimationAriaLabel", IDS_OOBE_PAUSE_ANIMATION_MESSAGE}};
  source->AddLocalizedStrings(kAnimationMessage);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"floatingWorkspaceStartupDialogTitle",
       IDS_FLOATING_WORKSPACE_STARTUP_DIALOG_TITLE},
      {"floatingWorkspaceStartupDialogLongResponseTitle",
       IDS_FLOATING_WORKSPACE_STARTUP_DIALOG_LONG_RESPONSE_TITLE},
      {"floatingWorkspaceStartupDialogButton",
       IDS_FLOATING_WORKSPACE_STARTUP_DIALOG_BUTTON}};
  source->AddLocalizedStrings(kLocalizedStrings);

  OobeUI::AddOobeComponents(source);
  ash::EnableTrustedTypesCSP(source);
}

FloatingWorkspaceUI::~FloatingWorkspaceUI() = default;

}  // namespace ash
