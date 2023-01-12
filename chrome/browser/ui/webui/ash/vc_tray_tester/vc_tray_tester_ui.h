// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_VC_TRAY_TESTER_VC_TRAY_TESTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_VC_TRAY_TESTER_VC_TRAY_TESTER_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace ash {

// The UI controller for NotificationTester page.
class VcTrayTesterUI : public content::WebUIController {
 public:
  explicit VcTrayTesterUI(content::WebUI* web_ui);
  VcTrayTesterUI(const VcTrayTesterUI&) = delete;
  VcTrayTesterUI& operator=(const VcTrayTesterUI&) = delete;
  ~VcTrayTesterUI() override;
};

// UI config for the class above.
class VcTrayTesterUIConfig
    : public content::DefaultWebUIConfig<VcTrayTesterUI> {
 public:
  VcTrayTesterUIConfig();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_VC_TRAY_TESTER_VC_TRAY_TESTER_UI_H_
