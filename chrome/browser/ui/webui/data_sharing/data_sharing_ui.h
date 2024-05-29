// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class DataSharingUIConfig : public content::WebUIConfig {
 public:
  DataSharingUIConfig();
  ~DataSharingUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome-untrusted://data-sharing
class DataSharingUI : public TopChromeWebUIController {
 public:
  explicit DataSharingUI(content::WebUI* web_ui);
  ~DataSharingUI() override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_
