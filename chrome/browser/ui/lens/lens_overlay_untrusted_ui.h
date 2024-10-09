// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_UNTRUSTED_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/searchbox/searchbox.mojom-forward.h"

namespace ui {
class ColorChangeHandler;
}

namespace lens {
class LensOverlayUntrustedUI;

class LensOverlayUntrustedUIConfig
    : public DefaultTopChromeWebUIConfig<LensOverlayUntrustedUI> {
 public:
  LensOverlayUntrustedUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                    chrome::kChromeUILensOverlayHost) {}
};

// WebUI controller for the chrome-untrusted://lens-overlay page.
class LensOverlayUntrustedUI
    : public UntrustedTopChromeWebUIController,
      public lens::mojom::LensPageHandlerFactory,
      public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit LensOverlayUntrustedUI(content::WebUI* web_ui);

  LensOverlayUntrustedUI(const LensOverlayUntrustedUI&) = delete;
  LensOverlayUntrustedUI& operator=(const LensOverlayUntrustedUI&) = delete;
  ~LensOverlayUntrustedUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<lens::mojom::LensPageHandlerFactory> receiver);

  // Instantiates the implementor of the searchbox::mojom::PageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> receiver);

  // Instantiates the implementor of the
  // color_change_listener::mojom::PageHandler mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Instantiates the implementor of the help_bubble::mojom::HelpBubbleHandler
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  static constexpr std::string GetWebUIName() { return "LensOverlayUntrusted"; }

 private:
  // lens::mojom::LensPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensPage> page) override;
  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  mojo::Receiver<lens::mojom::LensPageHandlerFactory>
      lens_page_factory_receiver_{this};
  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  base::WeakPtrFactory<LensOverlayUntrustedUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace lens
#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_UNTRUSTED_UI_H_
