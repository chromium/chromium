// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_

#include <vector>

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/webapps/browser/installable/installable_metrics.h"

class Profile;

namespace web_app {

class WebApp;

class WebAppPublisherHelper {
 public:
  WebAppPublisherHelper(Profile* profile, apps::mojom::AppType app_type);
  WebAppPublisherHelper(const WebAppPublisherHelper&) = delete;
  WebAppPublisherHelper& operator=(const WebAppPublisherHelper&) = delete;
  ~WebAppPublisherHelper();

  // Indicates if |permission_type| is supported by Web Applications.
  static bool IsSupportedWebAppPermissionType(
      ContentSettingsType permission_type);

  // Populates the various show_in_* fields of |app|.
  void SetWebAppShowInFields(apps::mojom::AppPtr& app, const WebApp* web_app);

  // Appends |web_app| permissions to |target|.
  void PopulateWebAppPermissions(
      const WebApp* web_app,
      std::vector<apps::mojom::PermissionPtr>* target);

  // Creates an |apps::mojom::App| describing |web_app|.
  apps::mojom::AppPtr ConvertWebApp(const WebApp* web_app,
                                    apps::mojom::Readiness readiness);

  // Constructs an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr ConvertUninstalledWebApp(const WebApp* web_app);

  // Constructs an App with only the information required to update
  // last launch time.
  apps::mojom::AppPtr ConvertLaunchedWebApp(const WebApp* web_app);

  // Converts |uninstall_source| to a |WebappUninstallSource|.
  static webapps::WebappUninstallSource
  ConvertUninstallSourceToWebAppUninstallSource(
      apps::mojom::UninstallSource uninstall_source);

  // Directly uninstalls |web_app| without prompting the user.
  // If |clear_site_data| is true, any site data associated with the app will
  // be removed.
  // If |report_abuse| is true, the app will be reported for abuse to the Web
  // Store.
  void UninstallWebApp(const WebApp* web_app,
                       apps::mojom::UninstallSource uninstall_source,
                       bool clear_site_data,
                       bool report_abuse);

  Profile* profile() { return profile_; }

  apps::mojom::AppType app_type() const { return app_type_; }

 private:
  Profile* const profile_;

  // The app type of the publisher. The app type is kSystemWeb if the web apps
  // are serving from Lacros, and the app type is kWeb for all other cases.
  const apps::mojom::AppType app_type_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
