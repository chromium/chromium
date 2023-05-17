// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_common.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_desktop.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/chrome_untrusted_web_ui_configs_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RegisterChromeUntrustedWebUIConfigs() {
  // Don't add calls to `AddUntrustedWebUIConfig()` here. Add it in one of
  // the corresponding files:
  //  - chrome_untrusted_web_ui_configs_common.cc
  //  - chrome_untrusted_web_ui_configs_desktop.cc
  //  - chrome_untrusted_web_ui_configs_chromeos.cc

  RegisterCommonChromeUntrustedWebUIConfigs();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  RegisterDesktopChromeUntrustedWebUIConfigs();
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)  ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterAshChromeUntrustedWebUIConfigs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
