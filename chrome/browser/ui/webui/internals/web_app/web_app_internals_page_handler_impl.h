// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_WEB_APP_WEB_APP_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_WEB_APP_WEB_APP_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "chrome/browser/ui/webui/internals/web_app/web_app_internals.mojom.h"

class Profile;

namespace content {
class WebUIDataSource;
}

// Handles API requests from chrome://internals/web-app.
class WebAppInternalsPageHandlerImpl
    : public mojom::web_app_internals::WebAppInternalsPageHandler {
 public:
  explicit WebAppInternalsPageHandlerImpl(Profile* profile);
  WebAppInternalsPageHandlerImpl(const WebAppInternalsPageHandlerImpl&) =
      delete;
  WebAppInternalsPageHandlerImpl& operator=(
      const WebAppInternalsPageHandlerImpl&) = delete;
  ~WebAppInternalsPageHandlerImpl() override;

  static void AddPageResources(content::WebUIDataSource* source);

  // mojom::web_app_internals::WebAppInternalsPageHandler:
  void IsBmoEnabled(IsBmoEnabledCallback callback) override;
  void GetWebApps(GetWebAppsCallback callback) override;
  void GetPreinstalledWebAppDebugInfo(
      GetPreinstalledWebAppDebugInfoCallback callback) override;
  void GetExternallyInstalledWebAppPrefs(
      GetExternallyInstalledWebAppPrefsCallback callback) override;

 private:
  Profile* profile_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_WEB_APP_WEB_APP_INTERNALS_PAGE_HANDLER_IMPL_H_
