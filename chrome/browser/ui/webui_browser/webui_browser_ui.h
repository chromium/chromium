// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

class WebUIBrowserUI;

class WebUIBrowserUIConfig
    : public content::DefaultWebUIConfig<WebUIBrowserUI> {
 public:
  WebUIBrowserUIConfig();

  // content::DefaultWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://webui-browser
class WebUIBrowserUI : public ui::MojoWebUIController {
 public:
  explicit WebUIBrowserUI(content::WebUI* web_ui);
  ~WebUIBrowserUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_UI_H_
