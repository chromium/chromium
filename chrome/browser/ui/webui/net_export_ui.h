// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_EXPORT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_EXPORT_UI_H_

#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class NetExportUI;

class NetExportUIConfig : public content::DefaultWebUIConfig<NetExportUI> {
 public:
  NetExportUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINetExportHost) {}
};

// The C++ back-end for the chrome://net-export webui page.
class NetExportUI : public content::WebUIController {
 public:
  explicit NetExportUI(content::WebUI* web_ui);

  NetExportUI(const NetExportUI&) = delete;
  NetExportUI& operator=(const NetExportUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_EXPORT_UI_H_
