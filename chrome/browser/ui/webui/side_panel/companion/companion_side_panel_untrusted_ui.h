// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_SIDE_PANEL_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_SIDE_PANEL_UNTRUSTED_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"

class CompanionSidePanelUntrustedUI
    : public content::WebContentsObserver,
      public UntrustedTopChromeWebUIController,
      public side_panel::mojom::CompanionPageHandlerFactory {
 public:
  explicit CompanionSidePanelUntrustedUI(content::WebUI* web_ui);

  CompanionSidePanelUntrustedUI(const CompanionSidePanelUntrustedUI&) = delete;
  CompanionSidePanelUntrustedUI& operator=(
      const CompanionSidePanelUntrustedUI&) = delete;
  ~CompanionSidePanelUntrustedUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<side_panel::mojom::CompanionPageHandlerFactory>
          receiver);

  // Gets a weak pointer to this object.
  base::WeakPtr<CompanionSidePanelUntrustedUI> GetWeakPtr();

  static constexpr std::string GetWebUIName() {
    return "CompanionSidePanelUntrusted";
  }

 private:
  // side_panel::mojom::CompanionPageHandlerFactory:
  void CreateCompanionPageHandler(
      mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::CompanionPage> page) override;

  // content::WebContentsObserver:
  // Listening to navigations because the primary document is the WebUI frame
  // which is able to load without network access.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::unique_ptr<companion::CompanionPageHandler> companion_page_handler_;
  mojo::Receiver<side_panel::mojom::CompanionPageHandlerFactory>
      companion_page_factory_receiver_{this};

  base::WeakPtrFactory<CompanionSidePanelUntrustedUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// The configuration for the chrome-untrusted://companion-side-panel page.
class CompanionSidePanelUntrustedUIConfig
    : public DefaultTopChromeWebUIConfig<CompanionSidePanelUntrustedUI> {
 public:
  CompanionSidePanelUntrustedUIConfig();
  ~CompanionSidePanelUntrustedUIConfig() override = default;

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_SIDE_PANEL_UNTRUSTED_UI_H_
