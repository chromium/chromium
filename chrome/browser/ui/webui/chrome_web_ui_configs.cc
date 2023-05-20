// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/chrome_web_ui_configs_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RegisterChromeWebUIConfigs() {
  // Don't add calls to `AddWebUIConfig()` here. Add it in one of
  // the corresponding chrome_web_ui_configs_*.cc files. If an appropriate
  // file doesn't exist, please add one.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterAshChromeWebUIConfigs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
