// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVTOOLS_DEVTOOLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVTOOLS_DEVTOOLS_UI_H_

#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "content/public/browser/web_ui_controller.h"

class DevToolsUI : public content::WebUIController {
 public:
  static GURL GetProxyURL(const std::string& frontend_url);
  static GURL GetRemoteBaseURL();
  static bool IsFrontendResourceURL(const GURL& url);

  explicit DevToolsUI(content::WebUI* web_ui);

  DevToolsUI(const DevToolsUI&) = delete;
  DevToolsUI& operator=(const DevToolsUI&) = delete;

  ~DevToolsUI() override;

 private:
  DevToolsUIBindings bindings_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEVTOOLS_DEVTOOLS_UI_H_
