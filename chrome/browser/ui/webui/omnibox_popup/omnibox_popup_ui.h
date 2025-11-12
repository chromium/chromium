// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom-forward.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class WebuiOmniboxHandler;

namespace ui {
class ColorChangeHandler;
}  // namespace ui

class ComposeboxHandler;
class OmniboxPopupUI;

class OmniboxPopupUIConfig
    : public DefaultTopChromeWebUIConfig<OmniboxPopupUI> {
 public:
  OmniboxPopupUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUIOmniboxPopupHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The Web UI controller for the chrome://omnibox-popup.top-chrome.
class OmniboxPopupUI : public TopChromeWebUIController,
                       public composebox::mojom::PageHandlerFactory {
 public:
  explicit OmniboxPopupUI(content::WebUI* web_ui);
  OmniboxPopupUI(const OmniboxPopupUI&) = delete;
  OmniboxPopupUI& operator=(const OmniboxPopupUI&) = delete;
  ~OmniboxPopupUI() override;

  // Instantiates the implementor of the searchbox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(content::RenderFrameHost* host,
                     mojo::PendingReceiver<searchbox::mojom::PageHandler>
                         pending_page_handler);
  // Instantiates the implementor of color_change_listener::mojom::PageHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  // Instantiates the implementor of the
  // composebox::mojom::PageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver);

  // Instantiates the implementor of the composebox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void CreatePageHandler(
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<composebox::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override;

  WebuiOmniboxHandler* handler() { return handler_.get(); }

  static constexpr std::string_view GetWebUIName() { return "OmniboxPopup"; }

 private:
  std::unique_ptr<WebuiOmniboxHandler> handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  raw_ptr<Profile> profile_;

  std::unique_ptr<ComposeboxHandler> composebox_handler_;
  mojo::Receiver<composebox::mojom::PageHandlerFactory>
      composebox_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_UI_H_
