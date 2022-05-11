// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppInstallFinalizer;
class WebAppRegistrar;

// Install the web app after the manifest is retrieved and validated.
class WebAppInstallCommand : public WebAppCommand {
 public:
  WebAppInstallCommand(
      const AppId& app_id,
      Profile* profile,
      WebAppInstallFinalizer* install_finalizer,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      WebAppRegistrar* registrar,
      webapps::WebappInstallSource install_surface,
      base::WeakPtr<content::WebContents> contents,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      WebAppInstallFlow flow,
      absl::optional<WebAppInstallParams> install_params = absl::nullopt);
  ~WebAppInstallCommand() override;

  void Start() override;
  void OnBeforeForcedUninstallFromSync() override;
  void OnShutdown() override;

  content::WebContents* GetInstallingWebContents() override;

  base::Value ToDebugValue() const override;

 private:
  void Abort(webapps::InstallResultCode code);

  void OnInstallCompleted(const AppId& app_id, webapps::InstallResultCode code);

  WebAppInstallTask install_task_;

  base::WeakPtr<content::WebContents> web_contents_;
  WebAppInstallDialogCallback dialog_callback_;
  OnceInstallCallback install_callback_;

  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  blink::mojom::ManifestPtr opt_manifest_;
  GURL manifest_url_;
  WebAppInstallFlow flow_;

  AppId app_id_;
  absl::optional<WebAppInstallParams> install_params_;

  base::WeakPtrFactory<WebAppInstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
