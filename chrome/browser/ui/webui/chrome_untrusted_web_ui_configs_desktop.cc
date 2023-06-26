// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_desktop.h"

#include <memory>

#include "build/branding_buildflags.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/webui/feed/feed_ui_config.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"
#include "components/lens/buildflags.h"
#include "content/public/browser/webui_config_map.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/webui/lens/lens_untrusted_ui_config.h"
#endif

void RegisterDesktopChromeUntrustedWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();

  // Add untrusted `WebUIConfig`s common across all platforms excluding Android
  // here.
  map.AddUntrustedWebUIConfig(std::make_unique<feed::FeedUIConfig>());
  if (companion::IsCompanionFeatureEnabled()) {
    map.AddUntrustedWebUIConfig(
        std::make_unique<CompanionSidePanelUntrustedUIConfig>());
  }
  map.AddUntrustedWebUIConfig(
      std::make_unique<ReadAnythingUIUntrustedConfig>());

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  map.AddUntrustedWebUIConfig(std::make_unique<lens::LensUntrustedUIConfig>());
#endif
}
