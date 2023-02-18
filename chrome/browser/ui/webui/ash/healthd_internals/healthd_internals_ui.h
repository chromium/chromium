// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_HEALTHD_INTERNALS_HEALTHD_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_HEALTHD_INTERNALS_HEALTHD_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class HealthdInternalsUI;

// WebUIConfig for chrome://healthd-internals.
class HealthdInternalsUIConfig
    : public content::DefaultWebUIConfig<HealthdInternalsUI> {
 public:
  HealthdInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIHealthdInternalsHost) {}
};

// The UI controller for HealthdInternals page.
class HealthdInternalsUI : public ui::MojoWebUIController {
 public:
  explicit HealthdInternalsUI(content::WebUI* web_ui);

  HealthdInternalsUI(const HealthdInternalsUI&) = delete;
  HealthdInternalsUI& operator=(const HealthdInternalsUI&) = delete;

  ~HealthdInternalsUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_HEALTHD_INTERNALS_HEALTHD_INTERNALS_UI_H_
