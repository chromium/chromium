// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_UI_H_

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class DefaultBrowserModalUI;
class DefaultBrowserModalHandler;

namespace content {
class WebUI;
}

class DefaultBrowserModalUIConfig final
    : public content::DefaultWebUIConfig<DefaultBrowserModalUI> {
 public:
  // NOLINTNEXTLINE(modernize-use-equals-default)
  DefaultBrowserModalUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIDefaultBrowserModalHost) {}
};

// The WebUI controller for the chrome://default-browser-modal page.
class DefaultBrowserModalUI final : public TopChromeWebUIController {
 public:
  explicit DefaultBrowserModalUI(content::WebUI* web_ui);

  DefaultBrowserModalUI(const DefaultBrowserModalUI&) = delete;
  const DefaultBrowserModalUI& operator=(const DefaultBrowserModalUI&) = delete;

  ~DefaultBrowserModalUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_UI_H_
