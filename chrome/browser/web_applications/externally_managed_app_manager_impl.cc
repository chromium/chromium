// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace web_app {

struct ExternallyManagedAppManagerImpl::TaskAndCallback {
  TaskAndCallback(std::unique_ptr<ExternallyManagedAppInstallTask> task,
                  OnceInstallCallback callback)
      : task(std::move(task)), callback(std::move(callback)) {}
  ~TaskAndCallback() = default;

  std::unique_ptr<ExternallyManagedAppInstallTask> task;
  OnceInstallCallback callback;
};

ExternallyManagedAppManagerImpl::ExternallyManagedAppManagerImpl(
    Profile* profile)
    : profile_(profile), url_loader_(std::make_unique<WebAppUrlLoader>()) {}

ExternallyManagedAppManagerImpl::~ExternallyManagedAppManagerImpl() = default;

void ExternallyManagedAppManagerImpl::InstallNow(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  pending_installs_.push_front(std::make_unique<TaskAndCallback>(
      CreateInstallationTask(std::move(install_options)), std::move(callback)));

  PostMaybeStartNext();
}

void ExternallyManagedAppManagerImpl::Install(
    ExternalInstallOptions install_options,
    OnceInstallCallback callback) {
  pending_installs_.push_back(std::make_unique<TaskAndCallback>(
      CreateInstallationTask(std::move(install_options)), std::move(callback)));

  PostMaybeStartNext();
}

void ExternallyManagedAppManagerImpl::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  for (auto& install_options : install_options_list) {
    pending_installs_.push_back(std::make_unique<TaskAndCallback>(
        CreateInstallationTask(std::move(install_options)), callback));
  }

  PostMaybeStartNext();
}

void ExternallyManagedAppManagerImpl::UninstallApps(
    std::vector<GURL> uninstall_urls,
    ExternalInstallSource install_source,
    const UninstallCallback& callback) {
  for (auto& url : uninstall_urls) {
    finalizer()->UninstallExternalWebAppByUrl(
        url, ConvertExternalInstallSourceToSource(install_source),
        ConvertExternalInstallSourceToUninstallSource(install_source),
        base::BindOnce(
            [](const UninstallCallback& callback, const GURL& app_url,
               webapps::UninstallResultCode code) {
              callback.Run(app_url,
                           code == webapps::UninstallResultCode::kSuccess);
            },
            callback, url));
  }
}

void ExternallyManagedAppManagerImpl::Shutdown() {
  is_in_shutdown_ = true;
  pending_registrations_.clear();
  current_registration_.reset();
  pending_installs_.clear();
  url_loader_.reset();
  // `current_install_` keeps a pointer to `web_contents_` so destroy it before
  // releasing the WebContents.
  current_install_.reset();
  ReleaseWebContents();
}

void ExternallyManagedAppManagerImpl::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void ExternallyManagedAppManagerImpl::ReleaseWebContents() {
  DCHECK(pending_registrations_.empty());
  DCHECK(!current_registration_);
  DCHECK(pending_installs_.empty());
  DCHECK(!current_install_);

  web_contents_.reset();
}

std::unique_ptr<ExternallyManagedAppInstallTask>
ExternallyManagedAppManagerImpl::CreateInstallationTask(
    ExternalInstallOptions install_options) {
  return std::make_unique<ExternallyManagedAppInstallTask>(
      profile_, url_loader_.get(), ui_manager(), finalizer(),
      command_scheduler(), std::move(install_options));
}

std::unique_ptr<ExternallyManagedAppRegistrationTaskBase>
ExternallyManagedAppManagerImpl::StartRegistration(GURL install_url) {
  ExternallyManagedAppRegistrationTask::RegistrationCallback callback =
      base::BindOnce(&ExternallyManagedAppManagerImpl::OnRegistrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), install_url);
  return std::make_unique<ExternallyManagedAppRegistrationTask>(
      std::move(install_url), url_loader_.get(), web_contents_.get(),
      std::move(callback));
}

void ExternallyManagedAppManagerImpl::OnRegistrationFinished(
    const GURL& install_url,
    RegistrationResultCode result) {
  DCHECK_EQ(current_registration_->install_url(), install_url);
  ExternallyManagedAppManager::OnRegistrationFinished(install_url, result);

  current_registration_.reset();
  PostMaybeStartNext();
}

void ExternallyManagedAppManagerImpl::PostMaybeStartNext() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternallyManagedAppManagerImpl::MaybeStartNext,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExternallyManagedAppManagerImpl::MaybeStartNext() {
  if (current_install_ || is_in_shutdown_)
    return;
  command_scheduler()->ScheduleCallbackWithLock<FullSystemLock>(
      "ExternallyManagedAppManagerImpl::MaybeStartNext",
      std::make_unique<FullSystemLockDescription>(),
      base::BindOnce(
          &ExternallyManagedAppManagerImpl::MaybeStartNextOnLockAcquired,
          weak_ptr_factory_.GetWeakPtr()));
}

void ExternallyManagedAppManagerImpl::MaybeStartNextOnLockAcquired(
    FullSystemLock& lock) {
  if (current_install_ || is_in_shutdown_) {
    return;
  }

  while (!pending_installs_.empty()) {
    std::unique_ptr<TaskAndCallback> front =
        std::move(pending_installs_.front());
    pending_installs_.pop_front();

    const ExternalInstallOptions& install_options =
        front->task->install_options();

    if (install_options.force_reinstall) {
      StartInstallationTask(std::move(front));
      return;
    }

    absl::optional<AppId> app_id =
        lock.registrar().LookupExternalAppId(install_options.install_url);

    // If the URL is not in web_app registrar,
    // then no external source has installed it.
    if (!app_id.has_value()) {
      StartInstallationTask(std::move(front));
      return;
    }

    if (lock.registrar().IsInstalled(app_id.value())) {
      if (install_options.wait_for_windows_closed &&
          lock.ui_manager().GetNumWindowsForApp(app_id.value()) != 0) {
        lock.ui_manager().NotifyOnAllAppWindowsClosed(
            app_id.value(),
            base::BindOnce(&ExternallyManagedAppManagerImpl::Install,
                           weak_ptr_factory_.GetWeakPtr(), install_options,
                           std::move(front->callback)));
        continue;
      }

      // If the app is already installed, only reinstall it if the app is a
      // placeholder app and the client asked for it to be reinstalled.
      if (install_options.reinstall_placeholder &&
          lock.registrar().IsPlaceholderApp(
              app_id.value(), ConvertExternalInstallSourceToSource(
                                  install_options.install_source))) {
        StartInstallationTask(std::move(front));
        return;
      }

      // TODO(crbug.com/1300321): Investigate re-install of the app for all
      // WebAppManagement sources.
      if (ConvertExternalInstallSourceToSource(
              install_options.install_source) == WebAppManagement::kPolicy) {
        StartInstallationTask(std::move(front));
        return;
      } else {
        // Add install source before returning the result.
        ScopedRegistryUpdate update(&lock.sync_bridge());
        WebApp* app_to_update = update->UpdateApp(app_id.value());
        app_to_update->AddSource(ConvertExternalInstallSourceToSource(
            install_options.install_source));
        app_to_update->AddInstallURLToManagementExternalConfigMap(
            ConvertExternalInstallSourceToSource(
                install_options.install_source),
            install_options.install_url);
      }
      std::move(front->callback)
          .Run(install_options.install_url,
               ExternallyManagedAppManager::InstallResult(
                   webapps::InstallResultCode::kSuccessAlreadyInstalled,
                   app_id));
      continue;
    }

    // If neither of the above conditions applies, the app probably got
    // uninstalled but it wasn't been removed from the map. We should install
    // the app in this case.
    StartInstallationTask(std::move(front));
    return;
  }
  DCHECK(!current_install_);

  if (current_registration_ || RunNextRegistration())
    return;

  ReleaseWebContents();
}

void ExternallyManagedAppManagerImpl::StartInstallationTask(
    std::unique_ptr<TaskAndCallback> task) {
  DCHECK(!current_install_);
  DCHECK(!is_in_shutdown_);
  if (current_registration_) {
    // Preempt current registration.
    pending_registrations_.push_front(current_registration_->install_url());
    current_registration_.reset();
  }

  current_install_ = std::move(task);
  CreateWebContentsIfNecessary();
  current_install_->task->Install(
      web_contents_.get(),
      base::BindOnce(&ExternallyManagedAppManagerImpl::OnInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ExternallyManagedAppManagerImpl::RunNextRegistration() {
  if (pending_registrations_.empty()) {
    if (registrations_complete_callback_)
      std::move(registrations_complete_callback_).Run();
    return false;
  }

  GURL url_to_check = std::move(pending_registrations_.front());
  pending_registrations_.pop_front();
  current_registration_ = StartRegistration(std::move(url_to_check));
  return true;
}

void ExternallyManagedAppManagerImpl::CreateWebContentsIfNecessary() {
  DCHECK(!is_in_shutdown_);
  if (web_contents_)
    return;

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  CreateWebAppInstallTabHelpers(web_contents_.get());
}

void ExternallyManagedAppManagerImpl::OnInstalled(
    ExternallyManagedAppManager::InstallResult result) {
  if (result.app_id && IsSuccess(result.code)) {
    MaybeEnqueueServiceWorkerRegistration(
        current_install_->task->install_options());
  }

  // Post a task to avoid webapps::InstallableManager crashing and do so before
  // running the callback in case the callback tries to install another
  // app.
  PostMaybeStartNext();

  std::unique_ptr<TaskAndCallback> task_and_callback;
  task_and_callback.swap(current_install_);
  std::move(task_and_callback->callback)
      .Run(task_and_callback->task->install_options().install_url, result);
}

void ExternallyManagedAppManagerImpl::MaybeEnqueueServiceWorkerRegistration(
    const ExternalInstallOptions& install_options) {
  if (!base::FeatureList::IsEnabled(
          features::kDesktopPWAsCacheDuringDefaultInstall)) {
    return;
  }

  if (install_options.only_use_app_info_factory)
    return;

  if (!install_options.load_and_await_service_worker_registration)
    return;

  // TODO(crbug.com/809304): Call CreateWebContentsIfNecessary() instead of
  // checking web_contents_ once major migration of default hosted apps to web
  // apps has completed.
  // Temporarily using offline manifest migrations (in which |web_contents_|
  // is nullptr) in order to avoid overwhelming migrated-to web apps with hits
  // for service worker registrations.
  if (!web_contents_)
    return;

  GURL url = install_options.service_worker_registration_url.value_or(
      install_options.install_url);
  if (url.is_empty())
    return;
  if (url.scheme() == content::kChromeUIScheme)
    return;
  if (url.scheme() == content::kChromeUIUntrustedScheme)
    return;

  pending_registrations_.push_back(url);
}

}  // namespace web_app
