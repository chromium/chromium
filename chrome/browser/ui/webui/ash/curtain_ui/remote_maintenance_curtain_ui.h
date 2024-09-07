// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CURTAIN_UI_REMOTE_MAINTENANCE_CURTAIN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CURTAIN_UI_REMOTE_MAINTENANCE_CURTAIN_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class RemoteMaintenanceCurtainUI;

class RemoteMaintenanceCurtainUIConfig
    : public ChromeOSWebUIConfig<RemoteMaintenanceCurtainUI> {
 public:
  RemoteMaintenanceCurtainUIConfig()
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            chrome::kChromeUIRemoteManagementCurtainHost) {}
};

class RemoteMaintenanceCurtainUI : public ui::MojoWebUIController {
 public:
  explicit RemoteMaintenanceCurtainUI(content::WebUI* web_ui);

  RemoteMaintenanceCurtainUI(const RemoteMaintenanceCurtainUI&) = delete;
  RemoteMaintenanceCurtainUI& operator=(const RemoteMaintenanceCurtainUI&) =
      delete;

  ~RemoteMaintenanceCurtainUI() override;

  // Binds to the Jelly dynamic color Mojo
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CURTAIN_UI_REMOTE_MAINTENANCE_CURTAIN_UI_H_
