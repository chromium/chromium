// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_UI_H_

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class IwaDevPageHandler;
class IwaDevUI;

class IwaDevUIConfig : public content::DefaultWebUIConfig<IwaDevUI> {
 public:
  IwaDevUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIIwaDevHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome://iwa-dev.
class IwaDevUI : public ui::MojoWebUIController,
                 public iwa_dev::mojom::PageHandlerFactory {
 public:
  explicit IwaDevUI(content::WebUI* web_ui);

  IwaDevUI(const IwaDevUI&) = delete;
  IwaDevUI& operator=(const IwaDevUI&) = delete;

  ~IwaDevUI() override;

  // Binds the pending receiver for the PageHandlerFactory mojo interface to
  // this WebUI controller.
  void BindInterface(
      mojo::PendingReceiver<iwa_dev::mojom::PageHandlerFactory> receiver);

  IwaDevPageHandler* GetHandlerForTesting();

 private:
  // iwa_dev::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<iwa_dev::mojom::Page> page,
      mojo::PendingReceiver<iwa_dev::mojom::PageHandler> receiver) override;

  std::unique_ptr<IwaDevPageHandler> page_handler_;

  mojo::Receiver<iwa_dev::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_UI_H_
