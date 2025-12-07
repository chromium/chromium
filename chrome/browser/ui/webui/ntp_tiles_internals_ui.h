// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_TILES_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_TILES_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class NTPTilesInternalsUI;

class NTPTilesInternalsUIConfig
    : public content::DefaultWebUIConfig<NTPTilesInternalsUI> {
 public:
  NTPTilesInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINTPTilesInternalsHost) {}
};

// The implementation for the chrome://ntp-tiles-internals page.
class NTPTilesInternalsUI : public content::WebUIController {
 public:
  explicit NTPTilesInternalsUI(content::WebUI* web_ui);

  NTPTilesInternalsUI(const NTPTilesInternalsUI&) = delete;
  NTPTilesInternalsUI& operator=(const NTPTilesInternalsUI&) = delete;

  ~NTPTilesInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_TILES_INTERNALS_UI_H_
