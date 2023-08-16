// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace ash {

// The UI controller for ChromeOS Status Area Internals page.
class StatusAreaInternalsUI : public content::WebUIController {
 public:
  explicit StatusAreaInternalsUI(content::WebUI* web_ui);
  StatusAreaInternalsUI(const StatusAreaInternalsUI&) = delete;
  StatusAreaInternalsUI& operator=(const StatusAreaInternalsUI&) = delete;
  ~StatusAreaInternalsUI() override;
};

// UI config for the class above.
class StatusAreaInternalsUIConfig
    : public content::DefaultWebUIConfig<StatusAreaInternalsUI> {
 public:
  StatusAreaInternalsUIConfig();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_INTERNALS_STATUS_AREA_INTERNALS_UI_H_
