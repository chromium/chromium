// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/webui_config_map.h"
#include "printing/buildflags/buildflags.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "chrome/browser/ui/webui/hats/hats_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "components/compose/buildflags.h"
#include "components/lens/buildflags.h"
#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/ui/webui/compose/compose_untrusted_ui.h"
#endif  // BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"
#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"
#endif  // defined(TOOLKIT_VIEWS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui_untrusted.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/config/chrome_untrusted_web_ui_configs_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void RegisterChromeUntrustedWebUIConfigs() {
  // Don't add calls to `AddUntrustedWebUIConfig()` for ash-specific UIs here.
  // Add them in chrome_untrusted_web_ui_configs_chromeos.cc.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterAshChromeUntrustedWebUIConfigs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS) || BUILDFLAG(ENABLE_PRINT_PREVIEW)
  auto& map = content::WebUIConfigMap::GetInstance();
#endif  // defined(TOOLKIT_VIEWS) || BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if defined(TOOLKIT_VIEWS)
  map.AddUntrustedWebUIConfig(
      std::make_unique<CompanionSidePanelUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<lens::LensOverlayUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<lens::LensSidePanelUntrustedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<ReadAnythingUIUntrustedConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<HatsUIConfig>());
  map.AddUntrustedWebUIConfig(std::make_unique<DataSharingUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<PrivacySandboxDialogUntrustedUIConfig>());

#if BUILDFLAG(ENABLE_COMPOSE)
  map.AddUntrustedWebUIConfig(std::make_unique<ComposeUIUntrustedConfig>());
#endif  // BUILDFLAG(ENABLE_COMPOSE)
#endif  // defined(TOOLKIT_VIEWS)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  map.AddUntrustedWebUIConfig(
      std::make_unique<printing::PrintPreviewUIUntrustedConfig>());
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
}
