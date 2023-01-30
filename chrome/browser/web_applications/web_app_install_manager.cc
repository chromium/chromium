// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <iterator>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_from_sync_command.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
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
  url_loader_.reset();
  web_contents_.reset();
}

bool WebAppInstallManager::IsInstallingForWebContents(
    const content::WebContents* web_contents) const {
  return base::ranges::any_of(
      tasks_, [web_contents](const std::unique_ptr<WebAppInstallTask>& task) {
        return task->GetInstallingWebContents() == web_contents;
      });
}

void WebAppInstallManager::SetSubsystems(
    WebAppRegistrar* registrar,
    OsIntegrationManager* os_integration_manager,
    WebAppCommandManager* command_manager,
    WebAppInstallFinalizer* finalizer,
    WebAppIconManager* icon_manager,
    WebAppSyncBridge* sync_bridge,
    WebAppTranslationManager* translation_manager) {
  registrar_ = registrar;
  os_integration_manager_ = os_integration_manager;
  command_manager_ = command_manager;
  finalizer_ = finalizer;
  icon_manager_ = icon_manager;
  sync_bridge_ = sync_bridge;
  translation_manager_ = translation_manager;
}

base::WeakPtr<WebAppInstallManager> WebAppInstallManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

  url_loader_->PrepareForLoad(
      web_contents_.get(),
      base::BindOnce(&WebAppInstallManager::OnWebContentsReadyRunTask,
                     GetWeakPtr(), std::move(pending_task)));
}

void WebAppInstallManager::TakeTaskErrorLog(WebAppInstallTask* task) {
  if (error_log_) {
    base::Value::Dict task_error_dict = task->TakeErrorDict();
    if (!task_error_dict.empty()) {
      LogErrorObject(std::move(task_error_dict));
    }
  }
}

void WebAppInstallManager::TakeCommandErrorLog(
    base::PassKey<WebAppCommandManager>,
    base::Value::Dict log) {
  if (error_log_)
    LogErrorObject(std::move(log));
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

  base::Value::Dict url_loader_error;
  url_loader_error.Set("WebAppUrlLoader::Result",
                       ConvertUrlLoaderResultToString(result));

  if (pending_task.task->app_id_to_expect().has_value()) {
    url_loader_error.Set("task.app_id_to_expect",
                         pending_task.task->app_id_to_expect().value());
  }

  LogErrorObjectAtStage(stage, std::move(url_loader_error));
}

void WebAppInstallManager::MaybeWriteErrorLog() {
  DCHECK(error_log_);
  if (error_log_writing_in_progress_ || !error_log_updated_)
    return;

  WriteErrorLog(GetWebAppsRootDirectory(profile_), kWebAppInstallManagerName,
                base::Value(error_log_->Clone()),
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
  *error_log_ = std::move(error_log).TakeList();

  // Appends the `early_error_log` at the end.
  error_log_->reserve(error_log_->size() + early_error_log.size());
  for (auto& error : early_error_log) {
    error_log_->Append(std::move(error));
  }
}

void WebAppInstallManager::LogErrorObject(base::Value::Dict object) {
  if (!error_log_)
    return;

  error_log_->Append(std::move(object));
  error_log_updated_ = true;
  MaybeWriteErrorLog();
}

void WebAppInstallManager::LogErrorObjectAtStage(const char* stage,
                                                 base::Value::Dict object) {
  if (!error_log_)
    return;

  object.Set("!stage", stage);
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
