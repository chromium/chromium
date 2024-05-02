// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_STATE_LOCAL_STATE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_STATE_LOCAL_STATE_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class LocalStateUI;

// WebUIConfig for chrome://local-state
class LocalStateUIConfig : public content::DefaultWebUIConfig<LocalStateUI> {
 public:
  LocalStateUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILocalStateHost) {}
};

// Controller for chrome://local-state/ page.
class LocalStateUI : public content::WebUIController {
 public:
  explicit LocalStateUI(content::WebUI* web_ui);

  LocalStateUI(const LocalStateUI&) = delete;
  LocalStateUI& operator=(const LocalStateUI&) = delete;

  ~LocalStateUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_STATE_LOCAL_STATE_UI_H_
