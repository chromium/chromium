// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"

namespace extensions {

namespace {

const int kSecondsToWaitForWebContentsLoad = 30;

std::unique_ptr<content::WebContents> WebContentsCreateWrapper(
    Profile* profile) {
  return content::WebContents::Create(
      content::WebContents::CreateParams(profile));
}

std::unique_ptr<BookmarkAppInstallationTask> InstallationTaskCreateWrapper(
    Profile* profile,
    web_app::PendingAppManager::AppInfo app_info) {
  return std::make_unique<BookmarkAppInstallationTask>(profile,
                                                       std::move(app_info));
}

}  // namespace

struct PendingBookmarkAppManager::TaskAndCallback {
  TaskAndCallback(std::unique_ptr<BookmarkAppInstallationTask> task,
                  OnceInstallCallback callback)
      : task(std::move(task)), callback(std::move(callback)) {}
  ~TaskAndCallback() = default;

  std::unique_ptr<BookmarkAppInstallationTask> task;
  OnceInstallCallback callback;
};

PendingBookmarkAppManager::PendingBookmarkAppManager(Profile* profile)
    : profile_(profile),
      extension_ids_map_(profile->GetPrefs()),
      web_contents_factory_(base::BindRepeating(&WebContentsCreateWrapper)),
      task_factory_(base::BindRepeating(&InstallationTaskCreateWrapper)),
      timer_(std::make_unique<base::OneShotTimer>()) {}

PendingBookmarkAppManager::~PendingBookmarkAppManager() = default;

void PendingBookmarkAppManager::Install(AppInfo app_to_install,
                                        OnceInstallCallback callback) {
  pending_tasks_and_callbacks_.push_front(std::make_unique<TaskAndCallback>(
      task_factory_.Run(profile_, std::move(app_to_install)),
      std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::InstallApps(
    std::vector<AppInfo> apps_to_install,
    const RepeatingInstallCallback& callback) {
  for (auto& app_to_install : apps_to_install) {
    pending_tasks_and_callbacks_.push_back(std::make_unique<TaskAndCallback>(
        task_factory_.Run(profile_, std::move(app_to_install)), callback));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingBookmarkAppManager::UninstallApps(
    std::vector<GURL> apps_to_uninstall,
    const UninstallCallback& callback) {
  for (auto& app_to_uninstall : apps_to_uninstall) {
    base::Optional<std::string> extension_id =
        extension_ids_map_.LookupExtensionId(app_to_uninstall);
    if (!extension_id) {
      callback.Run(app_to_uninstall, false);
      continue;
    }

    base::Optional<bool> opt =
        IsExtensionPresentAndInstalled(extension_id.value());
    if (!opt.has_value() || !opt.value()) {
      callback.Run(app_to_uninstall, false);
      continue;
    }

    DCHECK(opt.value());
    base::string16 error;
    bool uninstalled =
        ExtensionSystem::Get(profile_)->extension_service()->UninstallExtension(
            extension_id.value(), UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION,
            &error);

    if (!uninstalled) {
      LOG(WARNING) << "Couldn't uninstall app with url " << app_to_uninstall
                   << ". " << error;
    }

    callback.Run(app_to_uninstall, uninstalled);
  }
}

std::vector<GURL> PendingBookmarkAppManager::GetInstalledAppUrls(
    web_app::InstallSource install_source) const {
  return web_app::ExtensionIdsMap::GetInstalledAppUrls(profile_,
                                                       install_source);
}

void PendingBookmarkAppManager::SetFactoriesForTesting(
    WebContentsFactory web_contents_factory,
    TaskFactory task_factory) {
  web_contents_factory_ = std::move(web_contents_factory);
  task_factory_ = std::move(task_factory);
}

void PendingBookmarkAppManager::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

// Returns (as the base::Optional part) whether or not there is already a known
// extension for the given ID. The bool inside the base::Optional is, when
// known, whether the extension is installed (true) or uninstalled (false).
base::Optional<bool> PendingBookmarkAppManager::IsExtensionPresentAndInstalled(
    const std::string& extension_id) {
  if (ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING) != nullptr) {
    return base::Optional<bool>(true);
  }
  if (ExtensionPrefs::Get(profile_)->IsExternalExtensionUninstalled(
          extension_id)) {
    return base::Optional<bool>(false);
  }

  return base::nullopt;
}

void PendingBookmarkAppManager::MaybeStartNextInstallation() {
  if (current_task_and_callback_)
    return;

  while (!pending_tasks_and_callbacks_.empty()) {
    std::unique_ptr<TaskAndCallback> front =
        std::move(pending_tasks_and_callbacks_.front());
    pending_tasks_and_callbacks_.pop_front();

    const web_app::PendingAppManager::AppInfo& app_info =
        front->task->app_info();
    base::Optional<std::string> extension_id =
        extension_ids_map_.LookupExtensionId(app_info.url);

    if (extension_id) {
      base::Optional<bool> opt =
          IsExtensionPresentAndInstalled(extension_id.value());
      if (opt.has_value()) {
        bool installed = opt.value();
        if (installed || !app_info.override_previous_user_uninstall) {
          // TODO(crbug.com/878262): Handle the case where the app is already
          // installed but from a different source.
          std::move(front->callback)
              .Run(app_info.url,
                   installed
                       ? web_app::InstallResultCode::kAlreadyInstalled
                       : web_app::InstallResultCode::kPreviouslyUninstalled);
          continue;
        }
      }
    }

    current_task_and_callback_ = std::move(front);

    CreateWebContentsIfNecessary();
    Observe(web_contents_.get());

    content::NavigationController::LoadURLParams load_params(
        current_task_and_callback_->task->app_info().url);
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    web_contents_->GetController().LoadURLWithParams(load_params);
    timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kSecondsToWaitForWebContentsLoad),
        base::BindOnce(&PendingBookmarkAppManager::OnWebContentsLoadTimedOut,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  web_contents_.reset();
}

void PendingBookmarkAppManager::CreateWebContentsIfNecessary() {
  if (web_contents_)
    return;

  web_contents_ = web_contents_factory_.Run(profile_);
  BookmarkAppInstallationTask::CreateTabHelpers(web_contents_.get());
}

void PendingBookmarkAppManager::OnInstalled(
    BookmarkAppInstallationTask::Result result) {
  CurrentInstallationFinished(result.app_id);
}

void PendingBookmarkAppManager::OnWebContentsLoadTimedOut() {
  web_contents_->Stop();
  Observe(nullptr);
  CurrentInstallationFinished(base::nullopt);
}

void PendingBookmarkAppManager::CurrentInstallationFinished(
    const base::Optional<std::string>& app_id) {
  // Post a task to avoid reentrancy issues e.g. adding a WebContentsObserver
  // while a previous observer call is being executed. Post a task before
  // running the callback in case the callback tries to install another
  // app.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PendingBookmarkAppManager::MaybeStartNextInstallation,
                     weak_ptr_factory_.GetWeakPtr()));

  auto install_result_code = web_app::InstallResultCode::kFailedUnknownReason;
  if (app_id) {
    install_result_code = web_app::InstallResultCode::kSuccess;
    const auto& info = current_task_and_callback_->task->app_info();
    extension_ids_map_.Insert(info.url, app_id.value(), info.install_source);
  }

  std::unique_ptr<TaskAndCallback> task_and_callback;
  task_and_callback.swap(current_task_and_callback_);
  std::move(task_and_callback->callback)
      .Run(task_and_callback->task->app_info().url, install_result_code);
}

void PendingBookmarkAppManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  timer_->Stop();
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  if (validated_url != current_task_and_callback_->task->app_info().url) {
    CurrentInstallationFinished(base::nullopt);
    return;
  }

  Observe(nullptr);
  current_task_and_callback_->task->InstallWebAppOrShortcutFromWebContents(
      web_contents_.get(),
      base::BindOnce(&PendingBookmarkAppManager::OnInstalled,
                     // Safe because the installation task will not run its
                     // callback after being deleted and this class owns the
                     // task.
                     base::Unretained(this)));
}

void PendingBookmarkAppManager::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  timer_->Stop();
  if (web_contents_->GetMainFrame() != render_frame_host) {
    return;
  }

  Observe(nullptr);
  CurrentInstallationFinished(base::nullopt);
}

}  // namespace extensions
