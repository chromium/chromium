// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/webnn_internals/webnn_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class WebNNInternalsPageHandlerImpl;
class WebNNInternalsUI;

class WebNNInternalsUIConfig
    : public content::DefaultWebUIConfig<WebNNInternalsUI> {
 public:
  WebNNInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIWebNNInternalsHost) {}
};

class WebNNInternalsUI : public ui::MojoWebUIController,
                         public webnn_internals::mojom::PageHandlerFactory {
 public:
  explicit WebNNInternalsUI(content::WebUI* web_ui);
  ~WebNNInternalsUI() override;

  WebNNInternalsUI(const WebNNInternalsUI&) = delete;
  WebNNInternalsUI& operator=(const WebNNInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<webnn_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // webnn_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<webnn_internals::mojom::Page> page,
      mojo::PendingReceiver<webnn_internals::mojom::PageHandler> handler)
      override;

  std::unique_ptr<WebNNInternalsPageHandlerImpl> webnn_internals_page_handler_;
  mojo::Receiver<webnn_internals::mojom::PageHandlerFactory>
      webnn_internals_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBNN_INTERNALS_WEBNN_INTERNALS_UI_H_
