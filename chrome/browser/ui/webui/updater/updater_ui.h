// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class UpdaterUI;

class UpdaterUIConfig : public content::DefaultWebUIConfig<UpdaterUI> {
 public:
  UpdaterUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIUpdaterHost) {}
};

// The WebUI for chrome://updater.
class UpdaterUI : public content::WebUIController {
 public:
  explicit UpdaterUI(content::WebUI* web_ui);
  ~UpdaterUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_UI_H_
