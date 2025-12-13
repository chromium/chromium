// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

class CustomizeButtonsHandler;
class NewTabFooterHandler;
class NewTabFooterUI;
class PrefRegistrySimple;
class Profile;
class WebuiLoadTimer;

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
      public new_tab_footer::mojom::NewTabFooterHandlerFactory,
      public customize_buttons::mojom::CustomizeButtonsHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit NewTabFooterUI(content::WebUI* web_ui);
  ~NewTabFooterUI() override;

  static constexpr std::string_view GetWebUIName() { return "NewTabFooter"; }

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Instantiates the implementor of the mojom::NewTabFooterHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandlerFactory>
          pending_receiver);

  // Instantiates the implementor of the
  // customize_buttons::mojom::CustomizeButtonsHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<
                     customize_buttons::mojom::CustomizeButtonsHandlerFactory>
                         pending_receiver);

  // Instantiates the implementor of the
  // help_bubble::mojom::HelpBubbleHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  // Passthrough that calls the NewTabFooterDocument's AttachedTabStateUpdated.
  void AttachedTabStateUpdated(const GURL& url);

 private:
  // new_tab_footer::mojom::NewTabFooterHandlerFactory:
  void CreateNewTabFooterHandler(
      mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
          pending_document,
      mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
          pending_handler) override;

  // customize_buttons::mojom::CustomizeButtonsHandlerFactory:
  void CreateCustomizeButtonsHandler(
      mojo::PendingRemote<customize_buttons::mojom::CustomizeButtonsDocument>
          pending_page,
      mojo::PendingReceiver<customize_buttons::mojom::CustomizeButtonsHandler>
          pending_page_handler) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  GURL source_tab_url_;
  std::unique_ptr<WebuiLoadTimer> webui_load_timer_;
  std::unique_ptr<NewTabFooterHandler> handler_;
  mojo::Receiver<new_tab_footer::mojom::NewTabFooterHandlerFactory>
      document_factory_receiver_{this};
  std::unique_ptr<CustomizeButtonsHandler> customize_buttons_handler_;
  mojo::Receiver<customize_buttons::mojom::CustomizeButtonsHandlerFactory>
      customize_buttons_factory_receiver_;
  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};
  raw_ptr<Profile> profile_;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_NEW_TAB_FOOTER_UI_H_
