// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/fetch_installability_for_chrome_management.h"
#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"
#include "chrome/browser/web_applications/commands/manifest_update_finalize_command.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/installable/installable_metrics.h"

class GURL;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProvider;

// The command scheduler is the main API to access the web app system. The
// scheduler internally ensures:
// * Operations occur after the WebAppProvider is ready (so you don't have to
//   manually wait for this).
// * Operations are isolated from other operations in the system (currently
//   implemented using `WebAppCommand`s) to prevent race conditions while
//   reading/writing from the various data storage of the system.
// * Operations have the necessary dependencies from the WebAppProvider system.
class WebAppCommandScheduler {
 public:
  using ManifestFetchCallback =
      ManifestUpdateDataFetchCommand::ManifestFetchCallback;
  using ManifestWriteCallback =
      ManifestUpdateFinalizeCommand::ManifestWriteCallback;

  explicit WebAppCommandScheduler(WebAppProvider* provider);
  ~WebAppCommandScheduler();

  void Shutdown();

  // User initiated install that uses current `WebContents` to fetch manifest
  // and install the web app.
  void FetchManifestAndInstall(webapps::WebappInstallSource install_surface,
                               base::WeakPtr<content::WebContents> contents,
                               bool bypass_service_worker_check,
                               WebAppInstallDialogCallback dialog_callback,
                               OnceInstallCallback callback,
                               bool use_fallback);

  void PersistFileHandlersUserChoice(const AppId& app_id,
                                     bool allowed,
                                     base::OnceClosure callback);

  void UpdateFileHandlerOsIntegration(const AppId& app_id,
                                      base::OnceClosure callback);

  // Schedule a command that performs fetching data from the manifest
  // for a manifest update.
  void ScheduleManifestUpdateDataFetch(
      const GURL& url,
      const AppId& app_id,
      base::WeakPtr<content::WebContents> contents,
      ManifestFetchCallback callback);

  // Schedules a command that performs the data writes into the DB for
  // completion of the manifest update.
  void ScheduleManifestUpdateFinalize(
      const GURL& url,
      const AppId& app_id,
      WebAppInstallInfo install_info,
      bool app_identity_update_allowed,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
      ManifestWriteCallback callback);

  void FetchInstallabilityForChromeManagement(
      const GURL& url,
      base::WeakPtr<content::WebContents> web_contents,
      FetchInstallabilityForChromeManagementCallback callback);

  // TODO(https://crbug.com/1298130): expose all commands for web app
  // operations.

 private:
  raw_ptr<WebAppProvider, DanglingUntriaged> provider_;
  bool is_in_shutdown_ = false;

  base::WeakPtrFactory<WebAppCommandScheduler> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_COMMAND_SCHEDULER_H_
