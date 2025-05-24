// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_UI_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_base.h"
#include "chrome/browser/ui/webui/app_management/app_management_page_handler_factory.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom-forward.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
class WebAppSettingsUI;

class WebAppSettingsUIConfig
    : public content::DefaultWebUIConfig<WebAppSettingsUI> {
 public:
  WebAppSettingsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIWebAppSettingsHost) {}
};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// The WebUI for chrome://app-settings
class WebAppSettingsUI : public ui::MojoWebUIController,
                         public web_app::WebAppInstallManagerObserver {
 public:
  explicit WebAppSettingsUI(content::WebUI* web_ui);

  WebAppSettingsUI(const WebAppSettingsUI&) = delete;
  WebAppSettingsUI& operator=(const WebAppSettingsUI&) = delete;

  ~WebAppSettingsUI() override;

  static std::unique_ptr<AppManagementPageHandlerBase::Delegate>
  CreateAppManagementPageHandlerDelegate(Profile* profile);

  // Instantiates implementor of the mojom::PageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<app_management::mojom::PageHandlerFactory>
          receiver);

  // WebAppInstallManagerObserver:
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  std::unique_ptr<AppManagementPageHandlerFactory>
      app_management_page_handler_factory_;
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_manager_observation_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_SETTINGS_WEB_APP_SETTINGS_UI_H_
