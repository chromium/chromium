// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_chromeos.h"

#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if !defined(OFFICIAL_BUILD)
#include "ash/webui/demo_mode_app_ui/demo_mode_app_ui.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#endif  // !defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
#if BUILDFLAG(IS_CHROMEOS_ASH)
void RegisterAshChromeWebUIConfigs() {
  // Add `WebUIConfig`s for Ash ChromeOS to the list here.
#if !defined(OFFICIAL_BUILD)
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddWebUIConfig(std::make_unique<ash::SampleSystemWebAppUIConfig>());
  map.AddWebUIConfig(std::make_unique<ash::DemoModeAppUIConfig>());
#endif  // !defined(OFFICIAL_BUILD)
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

void RegisterChromeOSChromeWebUIConfigs() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterAshChromeWebUIConfigs();
#endif
}
