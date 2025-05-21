// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INFOBAR_INTERNALS_INFOBAR_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INFOBAR_INTERNALS_INFOBAR_INTERNALS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/infobar_internals/infobar_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

class InfoBarInternalsUI;
class InfoBarInternalsHandler;

class InfobarInternalsUIConfig final
    : public content::DefaultInternalWebUIConfig<InfoBarInternalsUI> {
 public:
  InfobarInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIInfobarInternalsHost) {}
};

class InfoBarInternalsUI final
    : public ui::MojoWebUIController,
      public infobar_internals::mojom::PageHandlerFactory {
 public:
  explicit InfoBarInternalsUI(content::WebUI* web_ui);

  InfoBarInternalsUI(const InfoBarInternalsUI&) = delete;
  InfoBarInternalsUI& operator=(const InfoBarInternalsUI&) = delete;

  ~InfoBarInternalsUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<infobar_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // infobar_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<infobar_internals::mojom::Page> page,
      mojo::PendingReceiver<infobar_internals::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<InfoBarInternalsHandler> page_handler_;
  mojo::Receiver<infobar_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INFOBAR_INTERNALS_INFOBAR_INTERNALS_UI_H_
