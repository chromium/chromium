// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_WEB_APP_WITH_PARAMS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_WEB_APP_WITH_PARAMS_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppInstallFinalizer;
class WebAppRegistrar;

// Command to install web_apps from param by the ExternallyInstalledAppsManager
class InstallWebAppWithParamsCommand : public WebAppCommand {
 public:
  InstallWebAppWithParamsCommand(
      base::WeakPtr<content::WebContents> contents,
      const WebAppInstallParams& install_params,
      webapps::WebappInstallSource install_surface,
      WebAppInstallFinalizer* install_finalizer,
      WebAppRegistrar* registrar,
      OnceInstallCallback callback,
      std::unique_ptr<WebAppDataRetriever> data_retriever);
  ~InstallWebAppWithParamsCommand() override;

  void Start() override;
  void OnBeforeForcedUninstallFromSync() override;
  void OnShutdown() override;

  base::Value ToDebugValue() const override;

 private:
  void Abort(webapps::InstallResultCode code);

  void OnGetWebAppInstallInfoInCommand(
      std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    const GURL& manifest_url,
                                    bool valid_manifest_for_web_app,
                                    bool is_installable);

  base::WeakPtr<content::WebContents> web_contents_;
  WebAppInstallParams install_params_;
  webapps::WebappInstallSource install_surface_;
  base::raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  base::raw_ptr<WebAppRegistrar> registrar_;
  OnceInstallCallback install_callback_;

  bool bypass_service_worker_check_ = false;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  base::WeakPtrFactory<InstallWebAppWithParamsCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_WEB_APP_WITH_PARAMS_COMMAND_H_
