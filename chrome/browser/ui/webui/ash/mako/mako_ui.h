// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_

#include "chrome/browser/ash/input_method/mojom/editor.mojom.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

// WebUIConfig for chrome://mako
class MakoUntrustedUIConfig : public content::WebUIConfig {
 public:
  MakoUntrustedUIConfig();
  ~MakoUntrustedUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://mako
class MakoUntrustedUI : public ui::MojoWebUIController {
 public:
  static void Show();
  static void Hide();

  explicit MakoUntrustedUI(content::WebUI* web_ui);
  ~MakoUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<input_method::mojom::EditorInstance> receiver);

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash
#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
