// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_desktop.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/feed/feed_ui_config.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "components/lens/buildflags.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/webui/lens/lens_untrusted_ui_config.h"
#endif

void RegisterDesktopChromeUntrustedWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();

  // Add untrusted `WebUIConfig`s common across all platforms excluding Android
  // here.
  map.AddUntrustedWebUIConfig(std::make_unique<feed::FeedUIConfig>());
  if (base::FeatureList::IsEnabled(features::kSidePanelCompanion)) {
    map.AddUntrustedWebUIConfig(
        std::make_unique<CompanionSidePanelUntrustedUIConfig>());
  }
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  map.AddUntrustedWebUIConfig(std::make_unique<lens::LensUntrustedUIConfig>());
#endif
}
