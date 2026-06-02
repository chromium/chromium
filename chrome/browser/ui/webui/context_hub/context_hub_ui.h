// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_UI_H_

#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class ContextHubPageHandler;
class ContextHubUI;

class ContextHubUIConfig
    : public content::DefaultInternalWebUIConfig<ContextHubUI> {
 public:
  ContextHubUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIContextHubHost) {}
};

class ContextHubUI : public ui::MojoWebUIController,
                     public browser::context_hub::mojom::PageHandlerFactory {
 public:
  explicit ContextHubUI(content::WebUI* web_ui);
  ~ContextHubUI() override;

  ContextHubUI(const ContextHubUI&) = delete;
  ContextHubUI& operator=(const ContextHubUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<browser::context_hub::mojom::PageHandlerFactory>
          receiver);

 private:
  // browser::context_hub::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> handler)
      override;

  std::unique_ptr<ContextHubPageHandler> page_handler_;
  mojo::Receiver<browser::context_hub::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_UI_H_
