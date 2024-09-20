// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class WebApksUI;

class WebApksUIConfig : public content::DefaultWebUIConfig<WebApksUI> {
 public:
  WebApksUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIWebApksHost) {}
};

// The WebUI handler for chrome://webapks.
class WebApksUI : public content::WebUIController {
 public:
  explicit WebApksUI(content::WebUI* web_ui);

  WebApksUI(const WebApksUI&) = delete;
  WebApksUI& operator=(const WebApksUI&) = delete;

  ~WebApksUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBAPKS_WEBAPKS_UI_H_
