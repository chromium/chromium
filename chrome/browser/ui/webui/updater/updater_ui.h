// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class UpdaterUI;
class UpdaterPageHandler;

class UpdaterUIConfig : public content::DefaultWebUIConfig<UpdaterUI> {
 public:
  UpdaterUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIUpdaterHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://updater.
class UpdaterUI final : public ui::MojoWebUIController,
                        public updater_ui::mojom::PageHandlerFactory {
 public:
  explicit UpdaterUI(content::WebUI* web_ui);
  UpdaterUI(const UpdaterUI&) = delete;
  UpdaterUI& operator=(const UpdaterUI&) = delete;
  ~UpdaterUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<updater_ui::mojom::PageHandlerFactory> receiver);

 private:
  // updater_ui::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<updater_ui::mojom::Page> page,
      mojo::PendingReceiver<updater_ui::mojom::PageHandler> receiver) override;

  std::unique_ptr<UpdaterPageHandler> page_handler_;

  mojo::Receiver<updater_ui::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_UI_H_
