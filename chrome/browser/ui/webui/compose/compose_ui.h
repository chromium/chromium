// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

class ComposeUI : public ui::MojoWebUIController {
 public:
  explicit ComposeUI(content::WebUI* web_ui);
  ComposeUI(const ComposeUI&) = delete;
  ComposeUI& operator=(const ComposeUI&) = delete;
  ~ComposeUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_
