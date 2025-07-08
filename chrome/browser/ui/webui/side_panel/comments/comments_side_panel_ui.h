// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/browser/webui_config.h"

class CommentsSidePanelUI;

class CommentsSidePanelUIConfig
    : public DefaultTopChromeWebUIConfig<CommentsSidePanelUI> {
 public:
  CommentsSidePanelUIConfig();
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class CommentsSidePanelUI : public TopChromeWebUIController {
 public:
  explicit CommentsSidePanelUI(content::WebUI* web_ui);
  CommentsSidePanelUI(const CommentsSidePanelUI&) = delete;
  CommentsSidePanelUI& operator=(const CommentsSidePanelUI&) = delete;
  ~CommentsSidePanelUI() override;

  static constexpr std::string_view GetWebUIName() {
    return "CommentsSidePanel";
  }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_UI_H_
