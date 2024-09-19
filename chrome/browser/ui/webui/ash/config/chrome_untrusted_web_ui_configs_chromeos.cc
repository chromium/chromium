// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/config/chrome_untrusted_web_ui_configs_chromeos.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/eche_app_ui/untrusted_eche_app_ui.h"
#include "ash/webui/file_manager/file_manager_untrusted_ui.h"
#include "ash/webui/focus_mode/focus_mode_untrusted_ui.h"
#include "ash/webui/help_app_ui/help_app_kids_magazine_untrusted_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"
#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/annotator/untrusted_annotator_ui_config.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_app_untrusted_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/chrome_demo_mode_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/crosh_ui.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_untrusted_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/media_app_guest_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/projector_app/untrusted_projector_ui_config.h"
#include "chrome/browser/ash/system_web_apps/apps/terminal_ui.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/scalable_iph/scalable_iph_debug_ui.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"

#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_untrusted_ui.h"
#endif  // !defined(OFFICIAL_BUILD)

namespace ash {

std::unique_ptr<content::WebUIConfig> MakeDemoModeAppUntrustedUIConfig() {
  auto create_controller_func = base::BindRepeating(
      [](content::WebUI* web_ui,
         const GURL& url) -> std::unique_ptr<content::WebUIController> {
        return std::make_unique<DemoModeAppUntrustedUI>(
            web_ui, DemoSession::Get()->GetDemoAppComponentPath(),
            std::make_unique<ChromeDemoModeAppDelegate>(web_ui));
      });
  return std::make_unique<DemoModeAppUntrustedUIConfig>(create_controller_func);
}

void RegisterAshChromeUntrustedWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();
  // Add untrusted `WebUIConfig`s for Ash ChromeOS to the list here.
  //
  // All `WebUIConfig`s should be registered here, irrespective of whether their
  // `WebUI` is enabled or not. To conditionally enable/disable a WebUI,
  // developers should override `WebUIConfig::IsWebUIEnabled()`.
  map.AddUntrustedWebUIConfig(std::make_unique<BocaUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<CroshUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<TerminalUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<eche_app::UntrustedEcheAppUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<MediaAppGuestUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<HelpAppUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<CameraAppUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<HelpAppKidsMagazineUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<UntrustedProjectorUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<UntrustedAnnotatorUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<file_manager::FileManagerUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<feedback::OsFeedbackUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(MakeDemoModeAppUntrustedUIConfig());
  map.AddUntrustedWebUIConfig(std::make_unique<MakoUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<ScalableIphDebugUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<FocusModeUntrustedUIConfig>());
#if !defined(OFFICIAL_BUILD)
  map.AddUntrustedWebUIConfig(
      std::make_unique<SampleSystemWebAppUntrustedUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
}

}  // namespace ash
