// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_TESTER_STATUS_AREA_TESTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_TESTER_STATUS_AREA_TESTER_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace ash {

// The UI controller for ChromeOS Status Area test page.
class StatusAreaTesterUI : public content::WebUIController {
 public:
  explicit StatusAreaTesterUI(content::WebUI* web_ui);
  StatusAreaTesterUI(const StatusAreaTesterUI&) = delete;
  StatusAreaTesterUI& operator=(const StatusAreaTesterUI&) = delete;
  ~StatusAreaTesterUI() override;
};

// UI config for the class above.
class StatusAreaTesterUIConfig
    : public content::DefaultWebUIConfig<StatusAreaTesterUI> {
 public:
  StatusAreaTesterUIConfig();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_STATUS_AREA_TESTER_STATUS_AREA_TESTER_UI_H_
