// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emulator/device_emulator_ui.h"

#include <memory>

#include "ash/constants/webui_url_constants.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/emulator/device_emulator_message_handler.h"
#include "chrome/grit/emulator_resources.h"
#include "chrome/grit/emulator_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

// Create data source for chrome://device-emulator/.
void CreateAndAddDeviceEmulatorUIDataSource(content::WebUI* web_ui) {
  content::WebUIDataSource* html = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      ash::kChromeUIDeviceEmulatorHost);
  ash::EnableTrustedTypesCSP(html);

  // Add resources.
  html->AddResourcePaths(kEmulatorResources);
  html->SetDefaultResource(IDR_EMULATOR_DEVICE_EMULATOR_HTML);
}

}  // namespace

bool DeviceEmulatorUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return !base::SysInfo::IsRunningOnChromeOS();
}

DeviceEmulatorUI::DeviceEmulatorUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DeviceEmulatorMessageHandler>());
  CreateAndAddDeviceEmulatorUIDataSource(web_ui);
}

DeviceEmulatorUI::~DeviceEmulatorUI() = default;

}  // namespace ash
