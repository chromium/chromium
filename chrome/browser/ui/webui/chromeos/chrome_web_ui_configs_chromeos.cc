// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/chrome_untrusted_web_ui_configs_chromeos.h"

#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "chrome/browser/ash/web_applications/camera_app/chrome_camera_app_ui_delegate.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_manager_error_ui.h"
#include "chrome/browser/ui/webui/ash/account_manager/account_migration_welcome_ui.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/ash/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_ui.h"
#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

namespace {
using CreateWebUIControllerFunc =
    std::unique_ptr<content::WebUIController> (*)(content::WebUI*);

template <class Config, class Controller, class Delegate>
std::unique_ptr<content::WebUIConfig> MakeComponentConfig() {
  CreateWebUIControllerFunc create_controller_func =
      [](content::WebUI* web_ui) -> std::unique_ptr<content::WebUIController> {
    auto delegate = std::make_unique<Delegate>(web_ui);
    return std::make_unique<Controller>(web_ui, std::move(delegate));
  };

  return std::make_unique<Config>(create_controller_func);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterAshChromeWebUIConfigs() {
  // Add `WebUIConfig`s for Ash ChromeOS to the list here.
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(
      MakeComponentConfig<ash::CameraAppUIConfig, ash::CameraAppUI,
                          ChromeCameraAppUIDelegate>());
  map.AddWebUIConfig(std::make_unique<ash::ShortcutCustomizationAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::NotificationTesterUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AccountMigrationWelcomeUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AccountManagerErrorUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::BluetoothPairingDialogUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::AddSupervisionUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::CrostiniInstallerUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::CrostiniUpgraderUIConfig>());
#if !defined(OFFICIAL_BUILD)
  map.AddWebUIConfig(std::make_unique<ash::SampleSystemWebAppUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

void RegisterChromeOSChromeWebUIConfigs() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterAshChromeWebUIConfigs();
#endif
}
