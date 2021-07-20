// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_

#include <string>

#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/web_applications/app_service/web_apps_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace web_app {

// An app publisher (in the App Service sense) of Web Apps.
class WebApps : public WebAppsBase {
 public:
  WebApps(const mojo::Remote<apps::mojom::AppService>& app_service,
          Profile* profile);
  WebApps(const WebApps&) = delete;
  WebApps& operator=(const WebApps&) = delete;
  ~WebApps() override;

  // Uninstall for web apps on Chrome.
  static void UninstallImpl(WebAppProvider* provider,
                            const std::string& app_id,
                            apps::mojom::UninstallSource uninstall_source,
                            gfx::NativeWindow parent_window);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_
