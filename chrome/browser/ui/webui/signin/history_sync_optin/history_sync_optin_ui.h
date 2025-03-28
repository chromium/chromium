// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

class HistorySyncOptinHandler;
class HistorySyncOptinUI;

class HistorySyncOptinUIConfig
    : public content::DefaultWebUIConfig<HistorySyncOptinUI> {
 public:
  HistorySyncOptinUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIHistorySyncOptinHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class HistorySyncOptinUI
    : public ui::MojoWebUIController,
      public history_sync_optin::mojom::PageHandlerFactory {
 public:
  explicit HistorySyncOptinUI(content::WebUI* web_ui);
  ~HistorySyncOptinUI() override;

  HistorySyncOptinUI(const HistorySyncOptinUI&) = delete;
  HistorySyncOptinUI& operator=(const HistorySyncOptinUI&) = delete;

  // Instantiates the implementor of the
  // history_sync_optin::mojom::PageHandlerFactory mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandlerFactory>
          receiver);

 private:
  // history_sync_optin::mojom::PageHandlerFactory:
  void CreateHistorySyncOptinHandler(
      mojo::PendingRemote<history_sync_optin::mojom::Page> page,
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<HistorySyncOptinHandler> page_handler_;

  mojo::Receiver<history_sync_optin::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_UI_H_
