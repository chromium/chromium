// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {

bool TaskExpectsAppId(const WebAppInstallTask* task, const AppId& app_id) {
  return task && task->app_id_to_expect().has_value() &&
         task->app_id_to_expect().value() == app_id;
}

}  // namespace

WebAppInstallManager::WebAppInstallManager(Profile* profile)
    : profile_(profile), url_loader_(std::make_unique<WebAppUrlLoader>()) {
  data_retriever_factory_ = base::BindRepeating(
      []() { return std::make_unique<WebAppDataRetriever>(); });
}

WebAppInstallManager::~WebAppInstallManager() = default;

void WebAppInstallManager::Start() {
  DCHECK(!started_);
  started_ = true;
}

void WebAppInstallManager::Shutdown() {
  // Set the `started_` flag to false first so when we delete tasks below any
  // task that re-enters or uses this manager instance will see we're (going)
  // offline.
  started_ = false;

  tasks_.clear();
  {
    TaskQueue empty;
    task_queue_.swap(empty);
  }
  web_contents_.reset();
}

void WebAppInstallManager::SetSubsystems(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppInstallFinalizer* finalizer) {
  registrar_ = registrar;
  os_integration_manager_ = os_integration_manager;
  finalizer_ = finalizer;
}

void WebAppInstallManager::LoadWebAppAndCheckManifest(
    const GURL& web_app_url,
    webapps::WebappInstallSource install_source,
    WebAppManifestCheckCallback callback) {
  DCHECK(started_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);

  task->LoadWebAppAndCheckManifest(
      web_app_url, install_source, url_loader_.get(),
      base::BindOnce(
          &WebAppInstallManager::OnLoadWebAppAndCheckManifestCompleted,
          GetWeakPtr(), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromManifest(
    content::WebContents* contents,
    bool bypass_service_worker_check,
    webapps::WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  DCHECK(started_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);
  task->InstallWebAppFromManifest(
      contents, bypass_service_worker_check, install_source,
      std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     GetWeakPtr(), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    webapps::WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback) {
  DCHECK(started_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);
  task->InstallWebAppFromManifestWithFallback(
      contents, force_shortcut_app, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     GetWeakPtr(), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool overwrite_existing_manifest_fields,
    ForInstallableSite for_installable_site,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback callback) {
  InstallWebAppFromInfo(
      std::move(web_application_info), overwrite_existing_manifest_fields,
      for_installable_site, absl::nullopt, install_source, std::move(callback));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool overwrite_existing_manifest_fields,
    ForInstallableSite for_installable_site,
    const absl::optional<WebAppInstallParams>& install_params,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback callback) {
  DCHECK(started_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);
  if (install_params) {
    task->SetInstallParams(install_params.value());
  }
  task->InstallWebAppFromInfo(
      std::move(web_application_info), overwrite_existing_manifest_fields,
      for_installable_site, install_source,
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     GetWeakPtr(), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallWebAppWithParams(
    content::WebContents* web_contents,
    const WebAppInstallParams& install_params,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback callback) {
  DCHECK(started_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);
  task->InstallWebAppWithParams(
      web_contents, install_params, install_source,
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     GetWeakPtr(), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

base::WeakPtr<WebAppInstallManager> WebAppInstallManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppInstallManager::EnqueueInstallAppFromSync(
    const AppId& sync_app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    OnceInstallCallback callback) {
  DCHECK(started_);
#if defined(OS_CHROMEOS)
  DCHECK(AreAppsLocallyInstalledBySync());
#endif

  if (registrar_->IsInstalled(sync_app_id) ||
      // Note that we call the callback too early here: an enqueued task has not
      // yet installed the app. This is fine (for now) because |callback| is
      // only used in tests.
      IsAppIdAlreadyEnqueued(sync_app_id)) {
    std::move(callback).Run(sync_app_id,
                            InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  // If sync_app_id is not installed enqueue full background installation
  // flow.
  GURL start_url = web_application_info->start_url;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);

  task->ExpectAppId(sync_app_id);

  WebAppInstallParams params;
  params.force_reinstall = true;
  params.override_manifest_id = web_application_info->manifest_id;
  params.user_display_mode = web_application_info->user_display_mode;
  params.fallback_start_url = start_url;
  params.fallback_app_name = web_application_info->title;
  // If app is not locally installed then no OS integration like OS shortcuts.
  params.locally_installed = AreAppsLocallyInstalledBySync();
  params.add_to_applications_menu = AreAppsLocallyInstalledBySync();
  params.add_to_desktop = AreAppsLocallyInstalledBySync();
  // Never add the app to the quick launch bar after sync.
  params.add_to_quick_launch_bar = false;
  task->SetInstallParams(params);

  OnceInstallCallback task_completed_callback = base::BindOnce(
      &WebAppInstallManager::
          LoadAndInstallWebAppFromManifestWithFallbackCompleted_ForAppSync,
      GetWeakPtr(), sync_app_id, std::move(web_application_info),
      std::move(callback));

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::LoadAndInstallWebAppFromManifestWithFallback,
      task->GetWeakPtr(), start_url, EnsureWebContentsCreated(),
      base::Unretained(url_loader_.get()), webapps::WebappInstallSource::SYNC,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted, GetWeakPtr(),
                     task.get(), std::move(task_completed_callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

std::set<AppId> WebAppInstallManager::GetEnqueuedInstallAppIdsForTesting() {
  std::set<AppId> app_ids;
  if (current_queued_task_ &&
      current_queued_task_->app_id_to_expect().has_value()) {
    app_ids.insert(current_queued_task_->app_id_to_expect().value());
  }
  for (const std::unique_ptr<WebAppInstallTask>& task : tasks_) {
    if (task && task->app_id_to_expect().has_value())
      app_ids.insert(task->app_id_to_expect().value());
  }
  return app_ids;
}

bool WebAppInstallManager::IsAppIdAlreadyEnqueued(const AppId& app_id) const {
  if (TaskExpectsAppId(current_queued_task_, app_id))
    return true;

  for (const std::unique_ptr<WebAppInstallTask>& task : tasks_) {
    if (TaskExpectsAppId(task.get(), app_id))
      return true;
  }

  return false;
}

void WebAppInstallManager::UpdateWebAppFromInfo(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    bool redownload_app_icons,
    OnceInstallCallback callback) {
  DCHECK(started_);

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::UpdateWebAppFromInfo, task->GetWeakPtr(),
      EnsureWebContentsCreated(), app_id, std::move(web_application_info),
      redownload_app_icons,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted,
                     base::Unretained(this), task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps,
    RepeatingInstallCallback callback) {
  DCHECK(started_);

  if (disable_web_app_sync_install_for_testing_)
    return;

  for (WebApp* web_app : web_apps) {
    DCHECK(web_app->is_from_sync_and_pending_installation());

    auto web_application_info = std::make_unique<WebApplicationInfo>();
    web_application_info->manifest_id = web_app->manifest_id();
    web_application_info->start_url = web_app->start_url();
    web_application_info->title =
        base::UTF8ToUTF16(web_app->sync_fallback_data().name);
    web_application_info->scope = web_app->sync_fallback_data().scope;
    web_application_info->theme_color =
        web_app->sync_fallback_data().theme_color;
    web_application_info->user_display_mode = web_app->user_display_mode();
    web_application_info->manifest_icons =
        web_app->sync_fallback_data().icon_infos;

    EnqueueInstallAppFromSync(web_app->app_id(),
                              std::move(web_application_info), callback);
  }
}

void WebAppInstallManager::UninstallFromSyncBeforeRegistryUpdate(
    std::vector<AppId> web_apps) {
  DCHECK(started_);
  finalizer_->UninstallFromSyncBeforeRegistryUpdate(std::move(web_apps));
}

void WebAppInstallManager::UninstallFromSyncAfterRegistryUpdate(
    std::vector<std::unique_ptr<WebApp>> web_apps,
    RepeatingUninstallCallback callback) {
  DCHECK(started_);
  finalizer_->UninstallFromSyncAfterRegistryUpdate(std::move(web_apps),
                                                   std::move(callback));
}

void WebAppInstallManager::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

void WebAppInstallManager::SetUrlLoaderForTesting(
    std::unique_ptr<WebAppUrlLoader> url_loader) {
  url_loader_ = std::move(url_loader);
}

void WebAppInstallManager::
    LoadAndInstallWebAppFromManifestWithFallbackCompleted_ForAppSync(
        const AppId& sync_app_id,
        std::unique_ptr<WebApplicationInfo> web_application_info,
        OnceInstallCallback callback,
        const AppId& web_app_id,
        InstallResultCode code) {
  // TODO(loyso): Record |code| for this specific case in
  // Webapp.BookmarkAppInstalledAfterSyncResult UMA.
  if (IsSuccess(code)) {
    DCHECK_EQ(sync_app_id, web_app_id);
    std::move(callback).Run(web_app_id, code);
    return;
  }

  // The install task or web contents getting destroyed indicates we could be
  // shutting down; don't enqueue another task.
  if (code == InstallResultCode::kWebContentsDestroyed ||
      code == InstallResultCode::kInstallTaskDestroyed) {
    return;
  }

  // Install failed. Do the fallback install from info fetching just icon URLs.
  auto task = std::make_unique<WebAppInstallTask>(
      profile_, os_integration_manager_, finalizer_,
      data_retriever_factory_.Run(), registrar_);
  // Set the expect app id for fallback install too. This can avoid duplicate
  // installs.
  task->ExpectAppId(sync_app_id);

  WebAppInstallFinalizer::FinalizeOptions finalize_options;
  finalize_options.install_source = webapps::WebappInstallSource::SYNC;
  finalize_options.locally_installed = AreAppsLocallyInstalledBySync();
  finalize_options.overwrite_existing_manifest_fields = true;

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons,
      task->GetWeakPtr(), EnsureWebContentsCreated(),
      std::move(web_application_info), finalize_options,
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted, GetWeakPtr(),
                     task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::EnqueueTask(std::unique_ptr<WebAppInstallTask> task,
                                       base::OnceClosure start_task) {
  DCHECK(web_contents_);

  PendingTask pending_task;
  pending_task.task = task.get();
  pending_task.start = std::move(start_task);
  task_queue_.push(std::move(pending_task));

  tasks_.insert(std::move(task));

  MaybeStartQueuedTask();
}

void WebAppInstallManager::MaybeStartQueuedTask() {
  if (!started_)
    return;

  DCHECK(web_contents_);

  if (current_queued_task_)
    return;

  DCHECK(!task_queue_.empty());
  PendingTask pending_task = std::move(task_queue_.front());
  task_queue_.pop();
  current_queued_task_ = pending_task.task;

  // Load about:blank to ensure ready and clean up any left over state.
  url_loader_->LoadUrl(
      GURL("about:blank"), web_contents_.get(),
      WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&WebAppInstallManager::OnWebContentsReadyRunTask,
                     GetWeakPtr(), std::move(pending_task)));
}

void WebAppInstallManager::DeleteTask(WebAppInstallTask* task) {
  DCHECK(tasks_.contains(task));
  tasks_.erase(task);
}

void WebAppInstallManager::OnInstallTaskCompleted(WebAppInstallTask* task,
                                                  OnceInstallCallback callback,
                                                  const AppId& app_id,
                                                  InstallResultCode code) {
  DeleteTask(task);

  std::move(callback).Run(app_id, code);
}

void WebAppInstallManager::OnQueuedTaskCompleted(WebAppInstallTask* task,
                                                 OnceInstallCallback callback,
                                                 const AppId& app_id,
                                                 InstallResultCode code) {
  DCHECK(current_queued_task_);
  DCHECK_EQ(current_queued_task_, task);
  current_queued_task_ = nullptr;

  OnInstallTaskCompleted(task, std::move(callback), app_id, code);
  // |task| is now destroyed.
  task = nullptr;

  if (task_queue_.empty() && !current_queued_task_)
    web_contents_.reset();
  else
    MaybeStartQueuedTask();
}

void WebAppInstallManager::OnLoadWebAppAndCheckManifestCompleted(
    WebAppInstallTask* task,
    WebAppManifestCheckCallback callback,
    std::unique_ptr<content::WebContents> web_contents,
    const AppId& app_id,
    InstallResultCode code) {
  DeleteTask(task);

  InstallableCheckResult result;
  absl::optional<AppId> opt_app_id;
  if (IsSuccess(code)) {
    if (!app_id.empty() && registrar_->IsInstalled(app_id)) {
      result = InstallableCheckResult::kAlreadyInstalled;
      opt_app_id = app_id;
    } else {
      result = InstallableCheckResult::kInstallable;
    }
  } else {
    result = InstallableCheckResult::kNotInstallable;
  }

  std::move(callback).Run(std::move(web_contents), result, opt_app_id);
}

content::WebContents* WebAppInstallManager::EnsureWebContentsCreated() {
  if (!web_contents_)
    web_contents_ = WebAppInstallTask::CreateWebContents(profile_);
  return web_contents_.get();
}

void WebAppInstallManager::OnWebContentsReadyRunTask(
    PendingTask pending_task,
    WebAppUrlLoader::Result result) {
  if (!web_contents_) {
    DCHECK(!started_);
    return;
  }

  DCHECK_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
  std::move(pending_task.start).Run();
}

WebAppInstallManager::PendingTask::PendingTask() = default;

WebAppInstallManager::PendingTask::PendingTask(PendingTask&&) = default;

WebAppInstallManager::PendingTask::~PendingTask() = default;

}  // namespace web_app
