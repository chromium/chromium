// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MULTISTEP_FILTER_INTERNALS_MULTISTEP_FILTER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MULTISTEP_FILTER_INTERNALS_MULTISTEP_FILTER_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace multistep_filter_internals {

class MultistepFilterInternalsPageHandler;

// WebUI controller for chrome://multistep-filter-internals.
// Serves as the factory for mojom::PageHandler, managing the Mojo connections
// between the browser process and the WebUI page.
class MultistepFilterInternalsUI : public ui::MojoWebUIController,
                                   public mojom::PageHandlerFactory {
 public:
  explicit MultistepFilterInternalsUI(content::WebUI* web_ui);

  MultistepFilterInternalsUI(const MultistepFilterInternalsUI&) = delete;
  MultistepFilterInternalsUI& operator=(const MultistepFilterInternalsUI&) =
      delete;

  ~MultistepFilterInternalsUI() override;

  // Binds the Mojo receiver for the PageHandlerFactory. This is called when
  // the WebUI page requests a connection to the PageHandlerFactory.
  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

  // mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> receiver) override;

 private:
  std::unique_ptr<MultistepFilterInternalsPageHandler> page_handler_;
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// WebUIConfig for chrome://multistep-filter-internals.
class MultistepFilterInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<MultistepFilterInternalsUI> {
 public:
  MultistepFilterInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            chrome::kChromeUIMultistepFilterInternalsHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace multistep_filter_internals

#endif  // CHROME_BROWSER_UI_WEBUI_MULTISTEP_FILTER_INTERNALS_MULTISTEP_FILTER_INTERNALS_UI_H_
