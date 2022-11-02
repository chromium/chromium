// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_HUMAN_PRESENCE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_HUMAN_PRESENCE_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class HumanPresenceInternalsUI;

// WebUIConfig for chrome://hps-internals
class HumanPresenceInternalsUIConfig
    : public content::DefaultWebUIConfig<HumanPresenceInternalsUI> {
 public:
  HumanPresenceInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIHumanPresenceInternalsHost) {}
};

// The WebUI for chrome://hps-internals.
class HumanPresenceInternalsUI : public content::WebUIController {
 public:
  explicit HumanPresenceInternalsUI(content::WebUI* web_ui);

  HumanPresenceInternalsUI(const HumanPresenceInternalsUI&) = delete;
  HumanPresenceInternalsUI& operator=(const HumanPresenceInternalsUI&) = delete;

  ~HumanPresenceInternalsUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_HUMAN_PRESENCE_INTERNALS_UI_H_
