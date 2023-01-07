// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_install_task.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class FullSystemLock;
class ExternallyManagedAppRegistrationTaskBase;

// Installs, uninstalls, and updates any External Web Apps. This class should
// only be used from the UI thread.
class ExternallyManagedAppManagerImpl : public ExternallyManagedAppManager {
 public:
  explicit ExternallyManagedAppManagerImpl(Profile* profile);
  ExternallyManagedAppManagerImpl(const ExternallyManagedAppManagerImpl&) =
      delete;
  ExternallyManagedAppManagerImpl& operator=(
      const ExternallyManagedAppManagerImpl&) = delete;
  ~ExternallyManagedAppManagerImpl() override;

  // ExternallyManagedAppManager:
  void InstallNow(ExternalInstallOptions install_options,
                  OnceInstallCallback callback) override;
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
  virtual void ReleaseWebContents();

  virtual std::unique_ptr<ExternallyManagedAppInstallTask>
  CreateInstallationTask(ExternalInstallOptions install_options);

  virtual std::unique_ptr<ExternallyManagedAppRegistrationTaskBase>
  StartRegistration(GURL launch_url);

  void OnRegistrationFinished(const GURL& launch_url,
                              RegistrationResultCode result) override;

  Profile* profile() { return profile_; }

 private:
  struct TaskAndCallback;

  void PostMaybeStartNext();

  void MaybeStartNext();
  void MaybeStartNextOnLockAcquired(FullSystemLock& lock);

  void StartInstallationTask(std::unique_ptr<TaskAndCallback> task);

  bool RunNextRegistration();

  void CreateWebContentsIfNecessary();

  void OnInstalled(ExternallyManagedAppManager::InstallResult result);

  void MaybeEnqueueServiceWorkerRegistration(
      const ExternalInstallOptions& install_options);

  const raw_ptr<Profile> profile_;

  bool is_in_shutdown_ = false;

  // unique_ptr so that it can be replaced in tests.
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<TaskAndCallback> current_install_;

  base::circular_deque<std::unique_ptr<TaskAndCallback>> pending_installs_;

  std::unique_ptr<ExternallyManagedAppRegistrationTaskBase>
      current_registration_;

  base::circular_deque<GURL> pending_registrations_;

  base::WeakPtrFactory<ExternallyManagedAppManagerImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_MANAGER_IMPL_H_
