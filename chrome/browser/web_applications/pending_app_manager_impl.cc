// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/pending_app_registration_task.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace web_app {

struct PendingAppManagerImpl::TaskAndCallback {
  TaskAndCallback(std::unique_ptr<PendingAppInstallTask> task,
                  OnceInstallCallback callback)
      : task(std::move(task)), callback(std::move(callback)) {}
  ~TaskAndCallback() = default;

  std::unique_ptr<PendingAppInstallTask> task;
  OnceInstallCallback callback;
};

PendingAppManagerImpl::PendingAppManagerImpl(Profile* profile)
    : profile_(profile),
      externally_installed_app_prefs_(profile->GetPrefs()),
      url_loader_(std::make_unique<WebAppUrlLoader>()) {}

PendingAppManagerImpl::~PendingAppManagerImpl() = default;

void PendingAppManagerImpl::Install(ExternalInstallOptions install_options,
                                    OnceInstallCallback callback) {
  pending_installs_.push_front(std::make_unique<TaskAndCallback>(
      CreateInstallationTask(std::move(install_options)), std::move(callback)));

  PostMaybeStartNext();
}

void PendingAppManagerImpl::InstallApps(
    std::vector<ExternalInstallOptions> install_options_list,
    const RepeatingInstallCallback& callback) {
  for (auto& install_options : install_options_list) {
    pending_installs_.push_back(std::make_unique<TaskAndCallback>(
        CreateInstallationTask(std::move(install_options)), callback));
  }

  PostMaybeStartNext();
}

void PendingAppManagerImpl::UninstallApps(std::vector<GURL> uninstall_urls,
                                          ExternalInstallSource install_source,
                                          const UninstallCallback& callback) {
  for (auto& url : uninstall_urls) {
    finalizer()->UninstallExternalWebAppByUrl(
        url, install_source,
        base::BindOnce(
            [](const UninstallCallback& callback, const GURL& app_url,
               bool uninstalled) { callback.Run(app_url, uninstalled); },
            callback, url));
  }
}

void PendingAppManagerImpl::Shutdown() {
  pending_registrations_.clear();
  current_registration_.reset();
  pending_installs_.clear();
  current_install_.reset();
  ReleaseWebContents();
}

void PendingAppManagerImpl::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void PendingAppManagerImpl::ReleaseWebContents() {
  DCHECK(pending_registrations_.empty());
  DCHECK(!current_registration_);
  DCHECK(pending_installs_.empty());
  DCHECK(!current_install_);

  web_contents_.reset();
}

std::unique_ptr<PendingAppInstallTask>
PendingAppManagerImpl::CreateInstallationTask(
    ExternalInstallOptions install_options) {
  return std::make_unique<PendingAppInstallTask>(
      profile_, registrar(), os_integration_manager(), ui_manager(),
      finalizer(), install_manager(), std::move(install_options));
}

std::unique_ptr<PendingAppRegistrationTaskBase>
PendingAppManagerImpl::StartRegistration(GURL install_url) {
  return std::make_unique<PendingAppRegistrationTask>(
      install_url, url_loader_.get(), web_contents_.get(),
      base::BindOnce(&PendingAppManagerImpl::OnRegistrationFinished,
                     weak_ptr_factory_.GetWeakPtr(), install_url));
}

void PendingAppManagerImpl::OnRegistrationFinished(
    const GURL& install_url,
    RegistrationResultCode result) {
  DCHECK_EQ(current_registration_->install_url(), install_url);
  PendingAppManager::OnRegistrationFinished(install_url, result);

  current_registration_.reset();
  PostMaybeStartNext();
}

void PendingAppManagerImpl::PostMaybeStartNext() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PendingAppManagerImpl::MaybeStartNext,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PendingAppManagerImpl::MaybeStartNext() {
  if (current_install_)
    return;

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

    base::Optional<AppId> app_id = externally_installed_app_prefs_.LookupAppId(
        install_options.install_url);

    // If the URL is not in ExternallyInstalledWebAppPrefs, then no external
    // source has installed it.
    if (!app_id.has_value()) {
      StartInstallationTask(std::move(front));
      return;
    }

    if (registrar()->IsInstalled(app_id.value())) {
      if (install_options.wait_for_windows_closed &&
          ui_manager()->GetNumWindowsForApp(app_id.value()) != 0) {
        ui_manager()->NotifyOnAllAppWindowsClosed(
            app_id.value(),
            base::BindOnce(&PendingAppManagerImpl::Install,
                           weak_ptr_factory_.GetWeakPtr(), install_options,
                           std::move(front->callback)));
        continue;
      }

      // If the app is already installed, only reinstall it if the app is a
      // placeholder app and the client asked for it to be reinstalled.
      if (install_options.reinstall_placeholder &&
          externally_installed_app_prefs_
              .LookupPlaceholderAppId(install_options.install_url)
              .has_value()) {
        StartInstallationTask(std::move(front));
        return;
      }

      // Otherwise no need to do anything.
      std::move(front->callback)
          .Run(install_options.install_url,
               InstallResultCode::kSuccessAlreadyInstalled);
      continue;
    }

    // The app is not installed, but it might have been previously uninstalled
    // by the user. If that's the case, don't install it again unless
    // |override_previous_user_uninstall| is true.
    if (finalizer()->WasExternalAppUninstalledByUser(app_id.value()) &&
        !install_options.override_previous_user_uninstall) {
      std::move(front->callback)
          .Run(install_options.install_url,
               InstallResultCode::kPreviouslyUninstalled);
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

void PendingAppManagerImpl::StartInstallationTask(
    std::unique_ptr<TaskAndCallback> task) {
  DCHECK(!current_install_);
  if (current_registration_) {
    // Preempt current registration.
    pending_registrations_.push_front(current_registration_->install_url());
    current_registration_.reset();
  }
  current_install_ = std::move(task);

  if (current_install_->task->install_options().only_use_app_info_factory) {
    DCHECK(current_install_->task->install_options().app_info_factory);
    current_install_->task->InstallFromInfo(base::BindOnce(
        &PendingAppManagerImpl::OnInstalled, weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  CreateWebContentsIfNecessary();

  url_loader_->PrepareForLoad(
      web_contents_.get(),
      base::BindOnce(&PendingAppManagerImpl::OnWebContentsReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingAppManagerImpl::OnWebContentsReady(WebAppUrlLoader::Result) {
  // TODO(crbug.com/1098139): Handle the scenario where WebAppUrlLoader fails to
  // load about:blank and flush WebContents states.
  url_loader_->LoadUrl(current_install_->task->install_options().install_url,
                       web_contents_.get(),
                       WebAppUrlLoader::UrlComparison::kSameOrigin,
                       base::BindOnce(&PendingAppManagerImpl::OnUrlLoaded,
                                      weak_ptr_factory_.GetWeakPtr()));
}

bool PendingAppManagerImpl::RunNextRegistration() {
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

void PendingAppManagerImpl::CreateWebContentsIfNecessary() {
  if (web_contents_)
    return;

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  PendingAppInstallTask::CreateTabHelpers(web_contents_.get());
}

void PendingAppManagerImpl::OnUrlLoaded(WebAppUrlLoader::Result result) {
  current_install_->task->Install(
      web_contents_.get(), result,
      base::BindOnce(&PendingAppManagerImpl::OnInstalled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingAppManagerImpl::OnInstalled(PendingAppInstallTask::Result result) {
  CurrentInstallationFinished(result.app_id, result.code);
}

void PendingAppManagerImpl::CurrentInstallationFinished(
    const base::Optional<AppId>& app_id,
    InstallResultCode code) {
  if (app_id && IsSuccess(code)) {
    MaybeEnqueueServiceWorkerRegistration(
        current_install_->task->install_options());
  }

  // Post a task to avoid InstallableManager crashing and do so before
  // running the callback in case the callback tries to install another
  // app.
  PostMaybeStartNext();

  std::unique_ptr<TaskAndCallback> task_and_callback;
  task_and_callback.swap(current_install_);
  std::move(task_and_callback->callback)
      .Run(task_and_callback->task->install_options().install_url, code);
}

void PendingAppManagerImpl::MaybeEnqueueServiceWorkerRegistration(
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
