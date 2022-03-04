// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_internals_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

namespace {

bool TaskExpectsAppId(const WebAppInstallTask* task, const AppId& app_id) {
  return task && task->app_id_to_expect().has_value() &&
         task->app_id_to_expect().value() == app_id;
}

constexpr char kWebAppInstallManagerName[] = "WebAppInstallManager";

}  // namespace

WebAppInstallManager::WebAppInstallManager(Profile* profile)
    : profile_(profile), url_loader_(std::make_unique<WebAppUrlLoader>()) {
  data_retriever_factory_ = base::BindRepeating(
      []() { return std::make_unique<WebAppDataRetriever>(); });
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    error_log_ = std::make_unique<ErrorLog>();
    ReadErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                 base::BindOnce(&WebAppInstallManager::OnReadErrorLog,
                                weak_ptr_factory_.GetWeakPtr()));
  } else {
    ClearErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                  base::DoNothing());
  }
}

WebAppInstallManager::~WebAppInstallManager() {
  NotifyWebAppInstallManagerDestroyed();
}

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

bool WebAppInstallManager::IsInstallingForWebContents(
    const content::WebContents* web_contents) const {
  return base::ranges::any_of(
      tasks_, [web_contents](const std::unique_ptr<WebAppInstallTask>& task) {
        return task->GetInstallingWebContents() == web_contents;
      });
}

std::size_t WebAppInstallManager::GetInstallTaskCountForTesting() const {
  return tasks_.size();
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
  if (!started_)
    return;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);

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
  if (!started_)
    return;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);
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
  if (!started_)
    return;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);
  task->InstallWebAppFromManifestWithFallback(
      contents, force_shortcut_app, install_source, std::move(dialog_callback),
      base::BindOnce(&WebAppInstallManager::OnInstallTaskCompleted,
                     GetWeakPtr(), task.get(), std::move(callback)));

  tasks_.insert(std::move(task));
}

void WebAppInstallManager::InstallSubApp(const AppId& parent_app_id,
                                         const GURL& install_url,
                                         OnceInstallCallback callback) {
  if (!started_)
    return;

  // Enqueue full background installation flow. Since app_id isn't available
  // yet, duplicate installation check will be performed down the line once
  // app_id is made available.

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);

  WebAppInstallParams params;
  params.parent_app_id = parent_app_id;
  params.require_manifest = true;
  params.add_to_quick_launch_bar = false;
  params.user_display_mode = blink::mojom::DisplayMode::kStandalone;
  params.fallback_start_url = install_url;
  // Don't want to allow devs to force manifest updates with the API.
  params.force_reinstall = false;

  task->SetInstallParams(params);

  base::OnceClosure start_task = base::BindOnce(
      &WebAppInstallTask::LoadAndInstallSubAppFromURL, task->GetWeakPtr(),
      install_url, EnsureWebContentsCreated(),
      base::Unretained(url_loader_.get()),
      base::BindOnce(&WebAppInstallManager::OnQueuedTaskCompleted, GetWeakPtr(),
                     task.get(), std::move(callback)));

  EnqueueTask(std::move(task), std::move(start_task));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebAppInstallInfo> web_application_info,
    bool overwrite_existing_manifest_fields,
    ForInstallableSite for_installable_site,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback callback) {
  InstallWebAppFromInfo(
      std::move(web_application_info), overwrite_existing_manifest_fields,
      for_installable_site, absl::nullopt, install_source, std::move(callback));
}

void WebAppInstallManager::InstallWebAppFromInfo(
    std::unique_ptr<WebAppInstallInfo> web_application_info,
    bool overwrite_existing_manifest_fields,
    ForInstallableSite for_installable_site,
    const absl::optional<WebAppInstallParams>& install_params,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback callback) {
  if (!started_)
    return;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);
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
  if (!started_)
    return;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);
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
    std::unique_ptr<WebAppInstallInfo> web_application_info,
    OnceInstallCallback callback) {
  DCHECK(started_);
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(AreAppsLocallyInstalledBySync());
#endif

  if (registrar_->IsInstalled(sync_app_id) ||
      // Note that we call the callback too early here: an enqueued task has not
      // yet installed the app. This is fine (for now) because |callback| is
      // only used in tests.
      IsAppIdAlreadyEnqueued(sync_app_id)) {
    std::move(callback).Run(
        sync_app_id, webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  // If sync_app_id is not installed enqueue full background installation
  // flow.
  GURL start_url = web_application_info->start_url;

  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);

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

void WebAppInstallManager::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps,
    RepeatingInstallCallback callback) {
  if (!started_)
    return;

  if (disable_web_app_sync_install_for_testing_)
    return;

  for (WebApp* web_app : web_apps) {
    DCHECK(web_app->is_from_sync_and_pending_installation());

    auto web_application_info = std::make_unique<WebAppInstallInfo>();
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

void WebAppInstallManager::UninstallWithoutRegistryUpdateFromSync(
    const std::vector<AppId>& web_apps,
    RepeatingUninstallCallback callback) {
  if (!started_)
    return;

  finalizer_->UninstallWithoutRegistryUpdateFromSync(
      std::move(web_apps),
      base::BindRepeating(
          [](RepeatingUninstallCallback callback, const web_app::AppId& app_id,
             webapps::UninstallResultCode code) {
            callback.Run(app_id,
                         code == webapps::UninstallResultCode::kSuccess);
          },
          std::move(callback)));
}

void WebAppInstallManager::RetryIncompleteUninstalls(
    const std::vector<AppId>& apps_to_uninstall) {
  if (!started_)
    return;

  finalizer_->RetryIncompleteUninstalls(apps_to_uninstall);
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
        std::unique_ptr<WebAppInstallInfo> web_application_info,
        OnceInstallCallback callback,
        const AppId& web_app_id,
        webapps::InstallResultCode code) {
  // TODO(loyso): Record |code| for this specific case in
  // Webapp.BookmarkAppInstalledAfterSyncResult UMA.
  if (IsSuccess(code)) {
    DCHECK_EQ(sync_app_id, web_app_id);
    std::move(callback).Run(web_app_id, code);
    return;
  }

  // The install task or web contents getting destroyed indicates we could be
  // shutting down; don't enqueue another task.
  if (code == webapps::InstallResultCode::kWebContentsDestroyed ||
      code == webapps::InstallResultCode::kInstallTaskDestroyed) {
    return;
  }

  // Install failed. Do the fallback install from info fetching just icon URLs.
  auto task = std::make_unique<WebAppInstallTask>(
      profile_, this, finalizer_, data_retriever_factory_.Run(), registrar_);
  // Set the expect app id for fallback install too. This can avoid duplicate
  // installs.
  task->ExpectAppId(sync_app_id);

  WebAppInstallFinalizer::FinalizeOptions finalize_options;
  finalize_options.install_source = webapps::WebappInstallSource::SYNC;
  finalize_options.overwrite_existing_manifest_fields = true;
  // If app is not locally installed then no OS integration like OS shortcuts.
  finalize_options.locally_installed = AreAppsLocallyInstalledBySync();
  finalize_options.add_to_applications_menu = AreAppsLocallyInstalledBySync();
  finalize_options.add_to_desktop = AreAppsLocallyInstalledBySync();
  // Never add the app to the quick launch bar after sync.
  finalize_options.add_to_quick_launch_bar = false;

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
  url_loader_->PrepareForLoad(
      web_contents_.get(),
      base::BindOnce(&WebAppInstallManager::OnWebContentsReadyRunTask,
                     GetWeakPtr(), std::move(pending_task)));
}

void WebAppInstallManager::TakeTaskErrorLog(WebAppInstallTask* task) {
  if (error_log_) {
    base::Value task_error_dict = task->TakeErrorDict();
    if (!task_error_dict.DictEmpty())
      LogErrorObject(std::move(task_error_dict));
  }
}

void WebAppInstallManager::DeleteTask(WebAppInstallTask* task) {
  TakeTaskErrorLog(task);
  // If this happens after/during the call to Shutdown(), then ignore deletion
  // as `tasks_` is emptied already.
  if (started_) {
    DCHECK(tasks_.contains(task));
    tasks_.erase(task);
  }
}

void WebAppInstallManager::OnInstallTaskCompleted(
    WebAppInstallTask* task,
    OnceInstallCallback callback,
    const AppId& app_id,
    webapps::InstallResultCode code) {
  DeleteTask(task);
  std::move(callback).Run(app_id, code);
}

void WebAppInstallManager::OnQueuedTaskCompleted(
    WebAppInstallTask* task,
    OnceInstallCallback callback,
    const AppId& app_id,
    webapps::InstallResultCode code) {
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
    webapps::InstallResultCode code) {
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

  // about:blank must always be loaded.
  DCHECK_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
  if (result != WebAppUrlLoader::Result::kUrlLoaded)
    LogUrlLoaderError("OnWebContentsReady", pending_task, result);

  std::move(pending_task.start).Run();
}

void WebAppInstallManager::LogUrlLoaderError(const char* stage,
                                             const PendingTask& pending_task,
                                             WebAppUrlLoader::Result result) {
  if (!error_log_)
    return;

  base::Value url_loader_error(base::Value::Type::DICTIONARY);

  url_loader_error.SetStringKey("WebAppUrlLoader::Result",
                                ConvertUrlLoaderResultToString(result));

  if (pending_task.task->app_id_to_expect().has_value()) {
    url_loader_error.SetStringKey(
        "task.app_id_to_expect", pending_task.task->app_id_to_expect().value());
  }

  LogErrorObjectAtStage(stage, std::move(url_loader_error));
}

void WebAppInstallManager::MaybeWriteErrorLog() {
  DCHECK(error_log_);
  if (error_log_writing_in_progress_ || !error_log_updated_)
    return;

  WriteErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                base::Value(*error_log_),
                base::BindOnce(&WebAppInstallManager::OnWriteErrorLog,
                               weak_ptr_factory_.GetWeakPtr()));

  error_log_writing_in_progress_ = true;
  error_log_updated_ = false;
}

void WebAppInstallManager::OnWriteErrorLog(Result result) {
  error_log_writing_in_progress_ = false;
  MaybeWriteErrorLog();
}

void WebAppInstallManager::OnReadErrorLog(Result result,
                                          base::Value error_log) {
  DCHECK(error_log_);
  if (result != Result::kOk || !error_log.is_list())
    return;

  ErrorLog early_error_log = std::move(*error_log_);
  *error_log_ = std::move(error_log).TakeListDeprecated();

  // Appends the `early_error_log` at the end.
  error_log_->insert(error_log_->end(),
                     std::make_move_iterator(early_error_log.begin()),
                     std::make_move_iterator(early_error_log.end()));
}

void WebAppInstallManager::LogErrorObject(base::Value object) {
  if (!error_log_)
    return;

  error_log_->push_back(std::move(object));
  error_log_updated_ = true;
  MaybeWriteErrorLog();
}

void WebAppInstallManager::LogErrorObjectAtStage(const char* stage,
                                                 base::Value object) {
  if (!error_log_)
    return;

  object.SetStringKey("!stage", stage);
  LogErrorObject(std::move(object));
}

void WebAppInstallManager::AddObserver(WebAppInstallManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppInstallManager::RemoveObserver(
    WebAppInstallManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppInstallManager::NotifyWebAppInstalled(const AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppInstalled(app_id);
  }
  // TODO(alancutter): Call RecordWebAppInstallation here when we get access to
  // the webapps::WebappInstallSource in this event.
}

void WebAppInstallManager::NotifyWebAppUninstalled(const AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppUninstalled(app_id);
  }
}

void WebAppInstallManager::NotifyWebAppManifestUpdated(
    const AppId& app_id,
    base::StringPiece old_name) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppManifestUpdated(app_id, old_name);
  }
}

void WebAppInstallManager::NotifyWebAppWillBeUninstalled(const AppId& app_id) {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppWillBeUninstalled(app_id);
  }
  RecordWebAppUninstallation(profile_->GetPrefs(), app_id);
}

void WebAppInstallManager::NotifyWebAppInstallManagerDestroyed() {
  for (WebAppInstallManagerObserver& observer : observers_) {
    observer.OnWebAppInstallManagerDestroyed();
  }
}

void WebAppInstallManager::NotifyWebAppInstalledWithOsHooks(
    const AppId& app_id) {
  for (WebAppInstallManagerObserver& obs : observers_) {
    obs.OnWebAppInstalledWithOsHooks(app_id);
  }
}

WebAppInstallManager::PendingTask::PendingTask() = default;

WebAppInstallManager::PendingTask::PendingTask(PendingTask&&) noexcept =
    default;

WebAppInstallManager::PendingTask::~PendingTask() = default;

}  // namespace web_app
