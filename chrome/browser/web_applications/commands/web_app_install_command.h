// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class WebAppDataRetriever;
class WebAppInstallFinalizer;

// Install the web app after the manifest is retrieved and validated.
class WebAppInstallCommand : public WebAppCommand {
 public:
  // When |dialog_callback| is null (aka |base::NullCallback|) the command
  // doesn't show installation prompt in UI and installs the application in
  // background.
  WebAppInstallCommand(const AppId& app_id,
                       webapps::WebappInstallSource install_surface,
                       std::unique_ptr<WebAppInstallInfo> web_app_info,
                       blink::mojom::ManifestPtr opt_manifest,
                       const GURL& manifest_url,
                       WebAppInstallFlow flow,
                       WebAppInstallDialogCallback dialog_callback,
                       OnceInstallCallback callback,
                       Profile* profile,
                       WebAppInstallFinalizer* install_finalizer,
                       std::unique_ptr<WebAppDataRetriever> data_retriever,
                       base::WeakPtr<content::WebContents> content);
  ~WebAppInstallCommand() override;

  Lock& lock() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  content::WebContents* GetInstallingWebContents() override;

  base::Value ToDebugValue() const override;

 private:
  bool IsWebContentsDestroyed();

  void Abort(webapps::InstallResultCode code);

  void OnInstallCompleted(const AppId& app_id, webapps::InstallResultCode code);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and an intent to the Play Store should be made, or
  // synchronously calls OnDidCheckForIntentToPlayStore() implicitly failing the
  // check if it cannot be made.
  void CheckForPlayStoreIntentOrGetIcons(base::flat_set<GURL> icon_urls,
                                         bool skip_page_favicons);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(base::flat_set<GURL> icon_urls,
                                      bool skip_page_favicons,
                                      const std::string& intent,
                                      bool should_intent_to_store);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns (Lacros adapter that calls
  // |OnDidCheckForIntentToPlayStore| based on |result|).
  void OnDidCheckForIntentToPlayStoreLacros(
      base::flat_set<GURL> icon_urls,
      bool skip_page_favicons,
      const std::string& intent,
      crosapi::mojom::IsInstallableResult result);
#endif

  void OnIconsRetrievedShowDialog(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnDialogCompleted(bool user_accepted,
                         std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnInstallFinalizedMaybeReparentTab(const AppId& app_id,
                                          webapps::InstallResultCode code,
                                          OsHooksErrors os_hooks_errors);

  std::unique_ptr<AppLock> lock_;
  AppId app_id_;
  webapps::WebappInstallSource install_surface_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  blink::mojom::ManifestPtr opt_manifest_;
  GURL manifest_url_;
  WebAppInstallFlow flow_;
  WebAppInstallDialogCallback dialog_callback_;
  OnceInstallCallback install_callback_;

  Profile* profile_;
  WebAppInstallFinalizer* install_finalizer_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::WeakPtr<content::WebContents> web_contents_;

  InstallErrorLogEntry install_error_log_entry_;

  base::WeakPtrFactory<WebAppInstallCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
