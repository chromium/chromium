// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace ash::extended_updates {

// The WebUIController for chrome://extended-updates-dialog
class ExtendedUpdatesUI : public content::WebUIController {
 public:
  explicit ExtendedUpdatesUI(content::WebUI* web_ui);
  ExtendedUpdatesUI(const ExtendedUpdatesUI&) = delete;
  ExtendedUpdatesUI& operator=(const ExtendedUpdatesUI&) = delete;
  ~ExtendedUpdatesUI() override;
};

// The WebUIConfig for chrome://extended-updates-dialog
class ExtendedUpdatesUIConfig
    : public content::DefaultWebUIConfig<ExtendedUpdatesUI> {
 public:
  ExtendedUpdatesUIConfig();
  ExtendedUpdatesUIConfig(const ExtendedUpdatesUIConfig&) = delete;
  ExtendedUpdatesUIConfig& operator=(const ExtendedUpdatesUIConfig&) = delete;
  ~ExtendedUpdatesUIConfig() override;

  // content::WebUIConfig overrides.
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace ash::extended_updates

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_UI_H_
