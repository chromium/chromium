// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace skills {

// MojoWebUIController for the chrome://skills page.
class SkillsUI : public ui::MojoWebUIController {
 public:
  explicit SkillsUI(content::WebUI* web_ui);
  SkillsUI(const SkillsUI&) = delete;
  SkillsUI& operator=(const SkillsUI&) = delete;

  ~SkillsUI() override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class SkillsUIConfig : public content::DefaultWebUIConfig<SkillsUI> {
 public:
  SkillsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISkillsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_UI_H_
