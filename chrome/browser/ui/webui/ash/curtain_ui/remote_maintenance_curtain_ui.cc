// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/curtain_ui/remote_maintenance_curtain_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/remote_maintenance_curtain_resources.h"
#include "chrome/grit/remote_maintenance_curtain_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_util.h"

namespace ash {

RemoteMaintenanceCurtainUI::RemoteMaintenanceCurtainUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIRemoteManagementCurtainHost);

  webui::SetupWebUIDataSource(source, kRemoteMaintenanceCurtainResources,
                              IDR_REMOTE_MAINTENANCE_CURTAIN_MAIN_HTML);

  // Add OOBE resources so our WebUI can find the OOBE WebUI resources (css,
  // javascript files, ...) at runtime.
  OobeUI::AddOobeComponents(source);

  // Add localized strings
  source->AddLocalizedString("curtainTitle", IDS_SECURITY_CURTAIN_TITLE);
  source->AddLocalizedString("curtainDescription",
                             IDS_SECURITY_CURTAIN_DESCRIPTION);
}

RemoteMaintenanceCurtainUI::~RemoteMaintenanceCurtainUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(RemoteMaintenanceCurtainUI)

}  // namespace ash
