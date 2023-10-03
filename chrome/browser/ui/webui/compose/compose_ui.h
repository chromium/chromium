// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}
class ComposeUI : public ui::MojoWebUIController {
 public:
  explicit ComposeUI(content::WebUI* web_ui);
  ComposeUI(const ComposeUI&) = delete;
  ComposeUI& operator=(const ComposeUI&) = delete;
  ~ComposeUI() override;

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

 private:
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_
