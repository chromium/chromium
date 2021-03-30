// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_INSTALL_TASK_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/components/web_application_info.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppUrlLoader;
class OsIntegrationManager;
class InstallFinalizer;
class InstallManager;
class WebAppUiManager;
enum class InstallResultCode;

// Class to install WebApp from a WebContents. A queue of such tasks is owned by
// PendingAppManager. Can only be called from the UI thread.
class PendingAppInstallTask {
 public:
  using ResultCallback =
      base::OnceCallback<void(base::Optional<AppId> app_id,
                              PendingAppManager::InstallResult result)>;

  // Ensures the tab helpers necessary for installing an app are present.
  static void CreateTabHelpers(content::WebContents* web_contents);

  // Constructs a task that will install a BookmarkApp-based Shortcut or Web App
  // for |profile|. |install_options| will be used to decide some of the
  // properties of the installed app e.g. open in a tab vs. window, installed by
  // policy, etc.
  explicit PendingAppInstallTask(Profile* profile,
                                 WebAppUrlLoader* url_loader,
                                 AppRegistrar* registrar,
                                 OsIntegrationManager* os_integration_manager,
                                 WebAppUiManager* ui_manager,
                                 InstallFinalizer* install_finalizer,
                                 InstallManager* install_manager,
                                 ExternalInstallOptions install_options);

  PendingAppInstallTask(const PendingAppInstallTask&) = delete;
  PendingAppInstallTask& operator=(const PendingAppInstallTask&) = delete;

  virtual ~PendingAppInstallTask();

  // Temporarily takes a |load_url_result| to decide if a placeholder app should
  // be installed.
  // TODO(ortuno): Remove once loading is done inside the task.
  virtual void Install(content::WebContents* web_contents,
                       ResultCallback result_callback);

  const ExternalInstallOptions& install_options() { return install_options_; }

 private:
  // Install directly from a fully specified WebApplicationInfo struct. Used
  // by system apps.
  void InstallFromInfo(ResultCallback result_callback);

  void OnWebContentsReady(content::WebContents* web_contents,
                          ResultCallback result_callback,
                          WebAppUrlLoader::Result prepare_for_load_result);
  void OnUrlLoaded(content::WebContents* web_contents,
                   ResultCallback result_callback,
                   WebAppUrlLoader::Result load_url_result);

  void InstallPlaceholder(ResultCallback result_callback);

  void UninstallPlaceholderApp(content::WebContents* web_contents,
                               ResultCallback result_callback);
  void OnPlaceholderUninstalled(content::WebContents* web_contents,
                                ResultCallback result_callback,
                                bool uninstalled);
  void ContinueWebAppInstall(content::WebContents* web_contents,
                             ResultCallback result_callback);
  void OnWebAppInstalled(bool is_placeholder,
                         bool offline_install,
                         ResultCallback result_callback,
                         const AppId& app_id,
                         InstallResultCode code);
  void TryAppInfoFactoryOnFailure(ResultCallback result_callback,
                                  base::Optional<AppId> app_id,
                                  PendingAppManager::InstallResult result);
  void OnOsHooksCreated(const AppId& app_id,
                        base::ScopedClosureRunner scoped_closure,
                        const OsHooksResults os_hooks_results);

  Profile* const profile_;
  WebAppUrlLoader* const url_loader_;
  AppRegistrar* const registrar_;
  OsIntegrationManager* const os_integration_manager_;
  InstallFinalizer* const install_finalizer_;
  InstallManager* const install_manager_;
  WebAppUiManager* const ui_manager_;

  ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  const ExternalInstallOptions install_options_;

  base::WeakPtrFactory<PendingAppInstallTask> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_INSTALL_TASK_H_
