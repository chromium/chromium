// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_SERVICE_INTERNALS_APP_SERVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_SERVICE_INTERNALS_APP_SERVICE_INTERNALS_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;
class AppServiceInternalsUI;

class AppServiceInternalsUIConfig
    : public content::DefaultWebUIConfig<AppServiceInternalsUI> {
 public:
  AppServiceInternalsUIConfig();
};

// The WebUI controller for chrome://app-service-internals.
class AppServiceInternalsUI : public ui::MojoWebUIController {
 public:
  explicit AppServiceInternalsUI(content::WebUI* web_ui);
  AppServiceInternalsUI(const AppServiceInternalsUI&) = delete;
  AppServiceInternalsUI& operator=(const AppServiceInternalsUI&) = delete;
  ~AppServiceInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          mojom::app_service_internals::AppServiceInternalsPageHandler>
          receiver);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  raw_ptr<Profile> profile_;
  std::unique_ptr<mojom::app_service_internals::AppServiceInternalsPageHandler>
      handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_SERVICE_INTERNALS_APP_SERVICE_INTERNALS_UI_H_
