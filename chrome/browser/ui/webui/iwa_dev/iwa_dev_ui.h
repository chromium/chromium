// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

class IwaDevUI;

class IwaDevUIConfig : public content::DefaultWebUIConfig<IwaDevUI> {
 public:
  IwaDevUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIIwaDevHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class IwaDevUI : public ui::MojoWebUIController {
 public:
  explicit IwaDevUI(content::WebUI* web_ui);

  IwaDevUI(const IwaDevUI&) = delete;
  IwaDevUI& operator=(const IwaDevUI&) = delete;

  ~IwaDevUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_UI_H_
