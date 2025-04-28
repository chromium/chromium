// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"

class NewTabFooterHandler;
class NewTabFooterUI;
class PrefRegistrySimple;
class Profile;

class NewTabFooterUIConfig
    : public DefaultTopChromeWebUIConfig<NewTabFooterUI> {
 public:
  NewTabFooterUIConfig();

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://newtab-footer
class NewTabFooterUI
    : public TopChromeWebUIController,
      public new_tab_footer::mojom::NewTabFooterHandlerFactory {
 public:
  explicit NewTabFooterUI(content::WebUI* web_ui);
  ~NewTabFooterUI() override;

  static constexpr std::string GetWebUIName() { return "NewTabFooter"; }

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Instantiates the implementor of the mojom::NewTabFooterHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandlerFactory>
          pending_receiver);

 private:
  // new_tab_footer::mojom::NewTabFooterHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
          pending_handler) override;

  std::unique_ptr<NewTabFooterHandler> handler_;
  mojo::Receiver<new_tab_footer::mojom::NewTabFooterHandlerFactory>
      document_factory_receiver_{this};
  raw_ptr<Profile> profile_;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_UI_H_
