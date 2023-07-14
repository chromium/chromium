// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emulator/device_emulator_ui.h"

#include <memory>

#include "ash/webui/common/trusted_types_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/emulator/device_emulator_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

// Create data source for chrome://device-emulator/.
void CreateAndAddDeviceEmulatorUIDataSource(content::WebUI* web_ui) {
  content::WebUIDataSource* html = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDeviceEmulatorHost);
  ash::EnableTrustedTypesCSP(html);

  // Add resources.
  html->AddResourcePath("audio_settings.js",
                        IDR_DEVICE_EMULATOR_AUDIO_SETTINGS_JS);
  html->AddResourcePath("battery_settings.js",
                        IDR_DEVICE_EMULATOR_BATTERY_SETTINGS_JS);
  html->AddResourcePath("bluetooth_settings.js",
                        IDR_DEVICE_EMULATOR_BLUETOOTH_SETTINGS_JS);
  html->AddResourcePath("icons.js", IDR_DEVICE_EMULATOR_ICONS_JS);
  html->AddResourcePath("input_device_settings.js",
                        IDR_DEVICE_EMULATOR_INPUT_DEVICE_SETTINGS_JS);
  html->AddResourcePath("device_emulator_pages.js",
                        IDR_DEVICE_EMULATOR_PAGES_JS);
  html->AddResourcePath("shared_styles.js",
                        IDR_DEVICE_EMULATOR_SHARED_STYLES_JS);
  html->AddResourcePath("device_emulator.css", IDR_DEVICE_EMULATOR_CSS);
  html->SetDefaultResource(IDR_DEVICE_EMULATOR_HTML);
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

DeviceEmulatorUI::~DeviceEmulatorUI() {}

}  // namespace ash
