// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_common.h"

#include "content/public/browser/webui_config_map.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void RegisterCommonChromeUntrustedWebUIConfigs() {
  // Add untrusted `WebUIConfig`s that are common to all platforms here.
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  auto& map = content::WebUIConfigMap::GetInstance();
  map.AddUntrustedWebUIConfig(
      std::make_unique<printing::PrintPreviewUIUntrustedConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
}
