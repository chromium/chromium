// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs_desktop.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/feed/feed_ui_config.h"
#include "chrome/browser/ui/webui/image_editor/image_editor_untrusted_ui.h"
#include "content/public/browser/webui_config_map.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/lens/lens_untrusted_ui_config.h"
#endif

void RegisterDesktopChromeUntrustedWebUIConfigs() {
  auto& map = content::WebUIConfigMap::GetInstance();

  // Add untrusted `WebUIConfig`s common across all platforms excluding Android
  // here.
  map.AddUntrustedWebUIConfig(std::make_unique<feed::FeedUIConfig>());
  map.AddUntrustedWebUIConfig(
      std::make_unique<image_editor::ImageEditorUntrustedUIConfig>());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  map.AddUntrustedWebUIConfig(std::make_unique<lens::LensUntrustedUIConfig>());
#endif
}
