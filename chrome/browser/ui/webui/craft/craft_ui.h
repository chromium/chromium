// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CRAFT_CRAFT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CRAFT_CRAFT_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace craft {

class CraftUI;

// WebUIConfig for chrome://craft.
class CraftUIConfig : public content::DefaultWebUIConfig<CraftUI> {
 public:
  CraftUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUIController for chrome://craft.
class CraftUI : public ui::MojoWebUIController {
 public:
  explicit CraftUI(content::WebUI* web_ui);
  ~CraftUI() override;

  CraftUI(const CraftUI&) = delete;
  CraftUI& operator=(const CraftUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace craft

#endif  // CHROME_BROWSER_UI_WEBUI_CRAFT_CRAFT_UI_H_
