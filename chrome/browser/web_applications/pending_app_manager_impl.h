// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/pending_app_install_task.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class PendingAppRegistrationTaskBase;

// Installs, uninstalls, and updates any External Web Apps. This class should
// only be used from the UI thread.
class PendingAppManagerImpl : public PendingAppManager {
 public:

  explicit PendingAppManagerImpl(Profile* profile);
  ~PendingAppManagerImpl() override;

  // PendingAppManager:
  void Install(ExternalInstallOptions install_options,
               OnceInstallCallback callback) override;
  void InstallApps(std::vector<ExternalInstallOptions> install_options_list,
                   const RepeatingInstallCallback& callback) override;
  void UninstallApps(std::vector<GURL> uninstall_urls,
                     ExternalInstallSource install_source,
                     const UninstallCallback& callback) override;
  void Shutdown() override;

  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> url_loader);

 protected:
  void ReleaseWebContents();

  virtual std::unique_ptr<PendingAppInstallTask> CreateInstallationTask(
      ExternalInstallOptions install_options);

  virtual std::unique_ptr<PendingAppRegistrationTaskBase> StartRegistration(
      GURL launch_url);

  void OnRegistrationFinished(const GURL& launch_url,
                              RegistrationResultCode result) override;

  Profile* profile() { return profile_; }

 private:
  struct TaskAndCallback;

  void PostMaybeStartNext();

  void MaybeStartNext();

  void StartInstallationTask(std::unique_ptr<TaskAndCallback> task);

  bool RunNextRegistration();

  void CreateWebContentsIfNecessary();

  void OnUrlLoaded(WebAppUrlLoader::Result result);

  void OnInstalled(PendingAppInstallTask::Result result);

  void CurrentInstallationFinished(const base::Optional<std::string>& app_id,
                                   InstallResultCode code);

  Profile* const profile_;
  ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  // unique_ptr so that it can be replaced in tests.
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<TaskAndCallback> current_install_;

  base::circular_deque<std::unique_ptr<TaskAndCallback>> pending_installs_;

  std::unique_ptr<PendingAppRegistrationTaskBase> current_registration_;

  base::circular_deque<GURL> pending_registrations_;

  base::WeakPtrFactory<PendingAppManagerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingAppManagerImpl);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_MANAGER_IMPL_H_
