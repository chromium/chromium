// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_REGIONAL_CAPABILITIES_INTERNALS_REGIONAL_CAPABILITIES_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_REGIONAL_CAPABILITIES_INTERNALS_REGIONAL_CAPABILITIES_INTERNALS_UI_H_

#include "components/webui/regional_capabilities_internals/constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"

// The WebUI controller for the chrome://regional-capabilities-internals page.
class RegionalCapabilitiesInternalsUI : public content::WebUIController {
 public:
  RegionalCapabilitiesInternalsUI(content::WebUI* web_ui, const GURL& url);

  RegionalCapabilitiesInternalsUI(const RegionalCapabilitiesInternalsUI&) =
      delete;
  RegionalCapabilitiesInternalsUI& operator=(
      const RegionalCapabilitiesInternalsUI&) = delete;
};

class RegionalCapabilitiesInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<
          RegionalCapabilitiesInternalsUI> {
 public:
  RegionalCapabilitiesInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            regional_capabilities::kChromeUIRegionalCapabilitiesInternalsHost) {
  }
};

#endif  // CHROME_BROWSER_UI_WEBUI_REGIONAL_CAPABILITIES_INTERNALS_REGIONAL_CAPABILITIES_INTERNALS_UI_H_
