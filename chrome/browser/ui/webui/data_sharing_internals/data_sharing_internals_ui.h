// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

class DataSharingUIConfig : public content::WebUIConfig {
 public:
  DataSharingUIConfig();
  ~DataSharingUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

// The WebUI controller for chrome://data-sharing-internals.
class DataSharingInternalsUI : public ui::MojoWebUIController {
 public:
  explicit DataSharingInternalsUI(content::WebUI* web_ui);
  ~DataSharingInternalsUI() override;

  DataSharingInternalsUI(const DataSharingInternalsUI&) = delete;
  DataSharingInternalsUI& operator=(const DataSharingInternalsUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_UI_H_
