// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYSTEM_SYSTEM_INFO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYSTEM_SYSTEM_INFO_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class SystemInfoUI;

class SystemInfoUIConfig : public content::DefaultWebUIConfig<SystemInfoUI> {
 public:
  SystemInfoUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISystemInfoHost) {}
};

class SystemInfoUI : public content::WebUIController {
 public:
  explicit SystemInfoUI(content::WebUI* web_ui);

  SystemInfoUI(const SystemInfoUI&) = delete;
  SystemInfoUI& operator=(const SystemInfoUI&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYSTEM_SYSTEM_INFO_UI_H_
