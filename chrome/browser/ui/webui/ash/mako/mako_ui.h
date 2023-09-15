// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_

#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

class BubbleContentsWrapper;
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
      mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver);

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// Handles showing and hiding the mako WebUI.
class MakoPageHandler {
 public:
  MakoPageHandler();
  MakoPageHandler(const MakoPageHandler&) = delete;
  MakoPageHandler& operator=(const MakoPageHandler&) = delete;
  ~MakoPageHandler();

  // TODO(b/300554470): Add parameters to specify when the WebUI is being opened
  // via a preset text query.
  void ShowConsentUI(Profile* profile);
  void ShowRewriteUI(Profile* profile);
  void CloseUI();

 private:
  // TODO(b/300554470): This doesn't seem like the right class to own the
  // contents wrapper and probably won't handle the bubble widget lifetimes
  // correctly. Figure out how WebUI bubbles work, then implement this properly
  // (maybe using a WebUIBubbleManager).
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MAKO_MAKO_UI_H_
