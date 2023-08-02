// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_

#include "chrome/browser/ash/input_method/mojom/editor.mojom.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

class Profile;

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
class MakoUntrustedUI : public ui::UntrustedBubbleWebUIController {
 public:
  static void Show(Profile* profile);
  explicit MakoUntrustedUI(content::WebUI* web_ui);
  ~MakoUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<input_method::mojom::EditorInstance> receiver);

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// Used by consumers to control the lifecycle of MakoUntrustedUI.
class MakoPageHandler {
 public:
  // Constructing an instance of this class will trigger the construction,
  // bootstrapping and showing of the MakoUntrustedUI WebUi bubble.
  MakoPageHandler();
  ~MakoPageHandler();

  // Consumers can use this method to close any currently visible
  // MakoUntrustedUI. Consumers cannot reshow the UI with this instance after
  // calling this method, a new instance must be created to reshow the UI.
  void CloseUI();
};

}  // namespace ash
#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
