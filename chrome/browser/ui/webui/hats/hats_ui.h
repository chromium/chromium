// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

// The configuration for the chrome-untrusted://hats page.
class HatsUIConfig : public content::WebUIConfig {
 public:
  HatsUIConfig();
  ~HatsUIConfig() override = default;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

class HatsUI : public ui::UntrustedWebUIController {
 public:
  explicit HatsUI(content::WebUI* web_ui);

  HatsUI(const HatsUI&) = delete;
  HatsUI& operator=(const HatsUI&) = delete;

  ~HatsUI() override = default;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_HATS_HATS_UI_H_
