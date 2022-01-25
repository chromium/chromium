// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_UI_H_

#include "chrome/browser/ui/webui/app_management/app_management_page_handler_factory.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom-forward.h"

// The WebUI for chrome://app-settings
class WebAppSettingsUI : public ui::MojoWebUIController {
 public:
  explicit WebAppSettingsUI(content::WebUI* web_ui);

  WebAppSettingsUI(const WebAppSettingsUI&) = delete;
  WebAppSettingsUI& operator=(const WebAppSettingsUI&) = delete;

  ~WebAppSettingsUI() override;

  // Instantiates implementor of the mojom::PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
          receiver);

 private:
  std::unique_ptr<AppManagementPageHandlerFactory>
      app_management_page_handler_factory_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_UI_H_
