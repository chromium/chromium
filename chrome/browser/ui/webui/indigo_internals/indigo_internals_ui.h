// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/indigo_internals/indigo_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class IndigoInternalsPageHandler;
class IndigoInternalsUI;

class IndigoInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<IndigoInternalsUI> {
 public:
  IndigoInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIIndigoInternalsHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class IndigoInternalsUI : public ui::MojoWebUIController,
                          public indigo_internals::mojom::PageHandlerFactory {
 public:
  explicit IndigoInternalsUI(content::WebUI* web_ui);
  ~IndigoInternalsUI() override;

  IndigoInternalsUI(const IndigoInternalsUI&) = delete;
  IndigoInternalsUI& operator=(const IndigoInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<indigo_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // indigo_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<indigo_internals::mojom::Page> page,
      mojo::PendingReceiver<indigo_internals::mojom::PageHandler> handler)
      override;

  std::unique_ptr<IndigoInternalsPageHandler> indigo_internals_page_handler_;
  mojo::Receiver<indigo_internals::mojom::PageHandlerFactory>
      indigo_internals_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INDIGO_INTERNALS_INDIGO_INTERNALS_UI_H_
