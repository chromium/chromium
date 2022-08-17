// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebUI;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

namespace webapps {

class AppHomePageHandler : public app_home::mojom::PageHandler {
 public:
  AppHomePageHandler(
      content::WebUI*,
      Profile* profile,
      mojo::PendingReceiver<app_home::mojom::PageHandler> receiver,
      mojo::PendingRemote<app_home::mojom::Page> page);

  AppHomePageHandler(const AppHomePageHandler&) = delete;
  AppHomePageHandler& operator=(const AppHomePageHandler&) = delete;

  ~AppHomePageHandler() override;

  // app_home::mojom::PageHandler:
  void GetApps(GetAppsCallback callback) override;

 private:
  void FillWebAppInfoList(std::vector<app_home::mojom::AppInfoPtr>* result);
  void FillExtensionInfoList(std::vector<app_home::mojom::AppInfoPtr>* result);
  app_home::mojom::AppInfoPtr CreateAppInfoPtrFromWebApp(
      const web_app::AppId& app_id);
  app_home::mojom::AppInfoPtr CreateAppInfoPtrFromExtension(
      const extensions::Extension* extension);

  raw_ptr<content::WebUI> web_ui_;

  raw_ptr<Profile> profile_;

  mojo::Receiver<app_home::mojom::PageHandler> receiver_;

  mojo::Remote<app_home::mojom::Page> page_;

  // Used for passing callbacks.
  base::WeakPtrFactory<AppHomePageHandler> weak_ptr_factory_{this};
};

}  // namespace webapps

#endif  // CHROME_BROWSER_UI_WEBUI_APP_HOME_APP_HOME_PAGE_HANDLER_H_
