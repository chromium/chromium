// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_INSTALL_TASK_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppShortcutManager;
class InstallFinalizer;
class WebAppUiManager;
enum class InstallResultCode;

// Class to install WebApp from a WebContents. A queue of such tasks is owned by
// PendingAppManager. Can only be called from the UI thread.
class PendingAppInstallTask {
 public:
  // TODO(loyso): Use InstallManager::OnceInstallCallback directly.
  struct Result {
    Result(InstallResultCode code, base::Optional<AppId> app_id);
    Result(Result&&);
    ~Result();

    const InstallResultCode code;
    const base::Optional<AppId> app_id;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  using ResultCallback = base::OnceCallback<void(Result)>;

  // Ensures the tab helpers necessary for installing an app are present.
  static void CreateTabHelpers(content::WebContents* web_contents);

  // Constructs a task that will install a BookmarkApp-based Shortcut or Web App
  // for |profile|. |install_options| will be used to decide some of the
  // properties of the installed app e.g. open in a tab vs. window, installed by
  // policy, etc.
  explicit PendingAppInstallTask(Profile* profile,
                                 AppRegistrar* registrar,
                                 AppShortcutManager* shortcut_manager,
                                 WebAppUiManager* ui_manager,
                                 InstallFinalizer* install_finalizer,
                                 ExternalInstallOptions install_options);

  virtual ~PendingAppInstallTask();

  // Temporarily takes a |load_url_result| to decide if a placeholder app should
  // be installed.
  // TODO(ortuno): Remove once loading is done inside the task.
  virtual void Install(content::WebContents* web_contents,
                       WebAppUrlLoader::Result load_url_result,
                       ResultCallback result_callback);

  const ExternalInstallOptions& install_options() { return install_options_; }

 private:
  void InstallPlaceholder(ResultCallback result_callback);

  void UninstallPlaceholderApp(content::WebContents* web_contents,
                               ResultCallback result_callback);
  void OnPlaceholderUninstalled(content::WebContents* web_contents,
                                ResultCallback result_callback,
                                bool uninstalled);
  void ContinueWebAppInstall(content::WebContents* web_contents,
                             ResultCallback result_callback);
  void OnWebAppInstalled(bool is_placeholder,
                         ResultCallback result_callback,
                         const AppId& app_id,
                         InstallResultCode code);

  Profile* const profile_;
  AppRegistrar* const registrar_;
  AppShortcutManager* const shortcut_manager_;
  InstallFinalizer* const install_finalizer_;
  WebAppUiManager* const ui_manager_;

  ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  const ExternalInstallOptions install_options_;

  base::WeakPtrFactory<PendingAppInstallTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingAppInstallTask);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_INSTALL_TASK_H_
