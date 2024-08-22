// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_GROWTH_INTERNALS_GROWTH_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_GROWTH_INTERNALS_GROWTH_INTERNALS_UI_H_

#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

class GrowthInternalsUI;

class GrowthInternalsUIConfig
    : public content::DefaultWebUIConfig<GrowthInternalsUI> {
 public:
  GrowthInternalsUIConfig()
      : content::DefaultWebUIConfig<GrowthInternalsUI>(
            content::kChromeUIUntrustedScheme,
            growth::kGrowthInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class GrowthInternalsUI : public ui::UntrustedWebUIController {
 public:
  explicit GrowthInternalsUI(content::WebUI* web_ui);
  ~GrowthInternalsUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_GROWTH_INTERNALS_GROWTH_INTERNALS_UI_H_
