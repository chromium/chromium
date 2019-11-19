// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom-forward.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_page_handler_factory.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUIMessageHandler;
}

namespace chromeos {
namespace settings {

// The WebUI handler for chrome://os-settings.
class OSSettingsUI : public ui::MojoWebUIController {
 public:
  explicit OSSettingsUI(content::WebUI* web_ui);
  ~OSSettingsUI() override;

 private:
  void AddSettingsPageUIHandler(
      std::unique_ptr<content::WebUIMessageHandler> handler);
  void BindCrosNetworkConfig(
      mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver);
  void BindAppManagementPageHandlerFactory(
      mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
          receiver);

  WebuiLoadTimer webui_load_timer_;

  std::unique_ptr<AppManagementPageHandlerFactory>
      app_management_page_handler_factory_;

  DISALLOW_COPY_AND_ASSIGN(OSSettingsUI);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_SETTINGS_UI_H_
