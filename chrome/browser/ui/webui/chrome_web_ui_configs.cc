// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/chrome_web_ui_configs_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RegisterChromeWebUIConfigs() {
  // Don't add calls to `AddWebUIConfig()` for Ash-specific WebUIs here. Add
  // them in chrome_web_ui_configs_chromeos.cc.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterAshChromeWebUIConfigs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_PRINT_PREVIEW)
  auto& map = content::WebUIConfigMap::GetInstance();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if !BUILDFLAG(IS_ANDROID)
  map.AddWebUIConfig(std::make_unique<BookmarksUIConfig>());
  map.AddWebUIConfig(std::make_unique<DownloadsUIConfig>());
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  map.AddWebUIConfig(std::make_unique<extensions::ExtensionsUIConfig>());
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  map.AddWebUIConfig(std::make_unique<printing::PrintPreviewUIConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
}
