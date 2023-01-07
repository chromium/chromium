// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_UI_H_

#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace webapps {
class AppHomePageHandler;
}  // namespace webapps

namespace webapps {

// The WebUI controller for chrome://apps.
class AppHomeUI : public ui::MojoWebUIController,
                  public app_home::mojom::PageHandlerFactory {
 public:
  explicit AppHomeUI(content::WebUI* web_ui);
  AppHomeUI(const AppHomeUI&) = delete;
  AppHomeUI& operator=(const AppHomeUI&) = delete;
  ~AppHomeUI() override;

  void BindInterface(
      mojo::PendingReceiver<app_home::mojom::PageHandlerFactory> receiver);

 private:
  // app_home::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<app_home::mojom::Page> page,
      mojo::PendingReceiver<app_home::mojom::PageHandler> receiver) override;

  std::unique_ptr<AppHomePageHandler> page_handler_;

  mojo::Receiver<app_home::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace webapps

#endif  // CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_UI_H_
