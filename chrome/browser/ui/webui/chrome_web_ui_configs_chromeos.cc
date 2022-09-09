// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_chromeos.h"

#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/face_ml_app_ui/face_ml_app_ui.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "chrome/browser/ui/webui/chromeos/notification_tester/notification_tester_ui.h"
#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterAshChromeWebUIConfigs() {
  // Add `WebUIConfig`s for Ash ChromeOS to the list here.
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<ash::ShortcutCustomizationAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<chromeos::NotificationTesterUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::FaceMLAppUIConfig>());
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
