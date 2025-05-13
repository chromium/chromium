// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_UI_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

class HistorySyncOptinHandler;
class HistorySyncOptinUI;
class Browser;
class Profile;

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

  // Prepares the information to be given to the handler once ready.
  void Initialize(Browser* browser);

 private:
  // history_sync_optin::mojom::PageHandlerFactory:
  void CreateHistorySyncOptinHandler(
      mojo::PendingRemote<history_sync_optin::mojom::Page> page,
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver)
      override;

  // Callback awaiting `CreateHistorySyncOptinHandler` to create the handlers
  // with all the needed information to display.
  void OnMojoHandlersReady(
      Browser* browser,
      mojo::PendingRemote<history_sync_optin::mojom::Page> page,
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver);

  // Callback that temporarily holds the information to be passed onto the
  // handler. The callback is called once the mojo handlers are available.
  base::OnceCallback<void(
      mojo::PendingRemote<history_sync_optin::mojom::Page>,
      mojo::PendingReceiver<history_sync_optin::mojom::PageHandler>)>
      initialize_handler_callback_;
  std::unique_ptr<HistorySyncOptinHandler> page_handler_;
  mojo::Receiver<history_sync_optin::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<HistorySyncOptinUI> weak_ptr_factory_{this};
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HISTORY_SYNC_OPTIN_UI_H_
