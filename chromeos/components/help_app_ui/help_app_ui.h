// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UI_H_
#define CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UI_H_

#include <memory>

#include "chromeos/components/help_app_ui/help_app_ui.mojom.h"
#include "chromeos/components/help_app_ui/help_app_ui_delegate.h"
#include "chromeos/components/local_search_service/local_search_service_proxy.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class HelpAppPageHandler;

namespace chromeos {

// The WebUI controller for chrome://help-app.
class HelpAppUI : public ui::MojoWebUIController,
                  public help_app_ui::mojom::PageHandlerFactory {
 public:
  HelpAppUI(content::WebUI* web_ui,
            std::unique_ptr<HelpAppUIDelegate> delegate);
  ~HelpAppUI() override;

  HelpAppUI(const HelpAppUI&) = delete;
  HelpAppUI& operator=(const HelpAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<help_app_ui::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<local_search_service::mojom::IndexProxy>
          index_receiver);

  HelpAppUIDelegate* delegate() { return delegate_.get(); }

 private:
  // help_app_ui::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<help_app_ui::mojom::PageHandler> receiver) override;

  std::unique_ptr<HelpAppPageHandler> page_handler_;
  mojo::Receiver<help_app_ui::mojom::PageHandlerFactory> page_factory_receiver_{
      this};
  std::unique_ptr<HelpAppUIDelegate> delegate_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UI_H_
