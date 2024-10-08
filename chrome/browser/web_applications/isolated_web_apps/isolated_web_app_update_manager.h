// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_waiter.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"

class GURL;
class Profile;

namespace web_package {
class SignedWebBundleId;
}

namespace web_app {

class IsolatedWebAppUrlInfo;
class IsolatedWebAppURLLoaderFactory;
class WebAppProvider;

namespace {
constexpr base::TimeDelta kDefaultUpdateDiscoveryFrequency = base::Hours(5);
}

// This enum lists the error types that can occur during the update of an
// isolated web apps.
//
// These values are persisted to logs and the values match the entries of
// `enum IsolatedWebAppUpdateError` in
// `tools/metrics/histograms/metadata/webapps/enums.xml`.
// Entries should not be renumbered and numeric values should never be reused.
enum class IsolatedWebAppUpdateError {
  kCantCalculateIsolatedWebAppUrlInfo = 1,
  kUpdateManifestDownloadFailed = 2,
  kUpdateManifestInvalidJson = 3,
  kUpdateManifestInvalidManifest = 4,
  kUpdateManifestNoApplicableVersion = 5,
  kIwaNotInstalled = 6,
  kDownloadPathCreationFailed = 7,
  kBundleDownloadError = 8,
  kUpdateDryRunFailed = 9,
  kUpdateApplyFailed = 10,
  kMaxValue = kUpdateApplyFailed
};

struct IsolatedWebAppUpdateOptions {
  GURL update_manifest_url;
  UpdateChannel update_channel;
};

// The `IsolatedWebAppUpdateManager` is responsible for discovery, download, and
// installation of Isolated Web App updates. Currently, it is only updating
// policy-installed IWAs on ChromeOS.
//
// TODO(crbug.com/40274186): Implement updates for unmanaged IWAs once we have
// designed that process.
//
// TODO(crbug.com/40274187): Consider only executing update discovery tasks when
// the user is not on a metered/paid internet connection.
class IsolatedWebAppUpdateManager
    : public WebAppInstallManagerObserver,
      public IwaKeyDistributionInfoProvider::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnUpdateDiscoveryTaskCompleted(
        const webapps::AppId& app_id,
        IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) {}

    // Will be invoked only if the discovery task finished with
    // `kUpdateFoundAndSavedInDatabase`.
    virtual void OnUpdateApplyTaskCompleted(
        const webapps::AppId& app_id,
        IsolatedWebAppUpdateApplyTask::CompletionStatus status) {}
  };

  explicit IsolatedWebAppUpdateManager(
      Profile& profile,
      base::TimeDelta update_discovery_frequency =
          kDefaultUpdateDiscoveryFrequency);
  ~IsolatedWebAppUpdateManager() override;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);

  void Start();

  // Called using `BEST_EFFORT` priority from `Start`. This is done so that we
  // don't overload the browser with update tasks during its startup process.
  void DelayedStart();

  void Shutdown();

  base::Value AsDebugValue() const;

  // Returns `true` if an update for the provided `app_id` is currently being
  // applied or scheduled to be applied soon.
  //
  // Use of this method should be limited to the
  // `IsolatedWebAppURLLoaderFactory`. If you have a different use case, please
  // talk to iwa-dev@chromium.org first.
  bool IsUpdateBeingApplied(base::PassKey<IsolatedWebAppURLLoaderFactory>,
                            const webapps::AppId app_id) const;

  // Starts an already scheduled update apply task for the provided `app_id`, if
  // it is queued but not already running. This happens regardless of whether
  // other update tasks are already running, and may therefore cause the task to
  // run concurrently with another running update task.
  //
  // `callback` will be run once the update apply task for the provided `app_id`
  // finishes.
  //
  // Use of this method should be limited to the
  // `IsolatedWebAppURLLoaderFactory`. If you have a different use case, please
  // talk to iwa-dev@chromium.org first.
  void PrioritizeUpdateAndWait(
      base::PassKey<IsolatedWebAppURLLoaderFactory>,
      const webapps::AppId& app_id,
      base::OnceCallback<void(IsolatedWebAppUpdateApplyTask::CompletionStatus)>
          callback);

  bool AreAutomaticUpdatesEnabled() const { return automatic_updates_enabled_; }

  void SetEnableAutomaticUpdatesForTesting(bool automatic_updates_enabled);

  // `WebAppInstallManagerObserver`:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;

  // Queues an update discovery task for the provided `app_id`, assuming that
  // the corresponding app is policy-installed (prod mode). Returns a boolean
  // indicating whether an update discovery task was queued successfully.
  bool MaybeDiscoverUpdatesForApp(const webapps::AppId& app_id);

  // Queues an update discovery task (and potentially an apply update task
  // afterwards if the discovery leads to a pending update) for the provided
  // `url_info.app_id`. The result of the discover & apply chain will be
  // communicated via observers.
  void DiscoverUpdatesForApp(const IsolatedWebAppUrlInfo& url_info,
                             const GURL& update_manifest_url,
                             const UpdateChannel& update_channel,
                             bool dev_mode);

  // Used to queue update discovery tasks manually from the
  // chrome://web-app-internals page. Returns the number of tasks queued.
  size_t DiscoverUpdatesNow();

  // Tells the update system about a locally available update for a dev-mode app
  // (as opposed to an update discovered through the Update Manifest of a
  // production app), and prioritizes applying it.
  void DiscoverApplyAndPrioritizeLocalDevModeUpdate(
      const IwaSourceDevModeWithFileOp& location,
      const IsolatedWebAppUrlInfo& url_info,
      base::OnceCallback<void(base::expected<base::Version, std::string>)>
          callback);

  std::optional<base::TimeTicks> GetNextUpdateDiscoveryTimeForTesting() const {
    return next_update_discovery_check_.GetScheduledTime();
  }

  void TrackResultOfUpdateDiscoveryTaskForTesting(
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) const {
    TrackResultOfUpdateDiscoveryTask(status);
  }

  void TrackResultOfUpdateApplyTaskForTesting(
      IsolatedWebAppUpdateApplyTask::CompletionStatus status) const {
    TrackResultOfUpdateApplyTask(status);
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // This queue manages update discovery and apply tasks. Tasks can be added to
  // the queue via its `Push` methods. The queue will never start a new task on
  // its own. Tasks can be started via `MaybeStartNextTask`; normally, only one
  // task is scheduled to run at the same time, with update apply tasks having
  // precedence over update discovery tasks. This is mainly to conserve
  // resources (because each update task requires a `WebContents`). However,
  // queued update apply tasks that are explicitly started via
  // `EnsureQueuedUpdateApplyTaskHasStarted` will run concurrently with other
  // potentially running tasks.
  class TaskQueue {
   public:
    explicit TaskQueue(IsolatedWebAppUpdateManager& update_manager);

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    ~TaskQueue();

    base::Value AsDebugValue() const;
    void ClearUpdateDiscoveryLog();

    void Push(std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask> task);
    void Push(std::unique_ptr<IsolatedWebAppUpdateApplyTask> task);
    void Clear();

    // If an `IsolatedWebAppUpdateApplyTask` for the `app_id` is queued, start
    // it immediately, even if other tasks are currently running. Returns
    // `false` if a task for this `app_id` is neither queued nor running,
    // returns `true` otherwise.
    bool EnsureQueuedUpdateApplyTaskHasStarted(const webapps::AppId& app_id);

    // Removes all tasks for the provided `app_id` that haven't yet started from
    // the queue.
    //
    // TODO(crbug.com/40267691): Ideally, we'd also cancel tasks that have
    // already started, especially update discovery tasks, but the task
    // implementation currently does not support cancellation of ongoing tasks.
    void ClearNonStartedTasksOfApp(const webapps::AppId& app_id);

    // Starts the next task if no task is currently running. Will prioritize
    // update apply over update discovery tasks.
    void MaybeStartNextTask();

    bool IsUpdateApplyTaskQueued(const webapps::AppId& app_id) const;

   private:
    void StartUpdateDiscoveryTask(IsolatedWebAppUpdateDiscoveryTask* task_ptr);

    void StartUpdateApplyTask(IsolatedWebAppUpdateApplyTask* task_ptr);

    bool IsAnyTaskRunning() const;

    void OnUpdateDiscoveryTaskCompleted(
        IsolatedWebAppUpdateDiscoveryTask* task_ptr,
        IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status);

    void OnUpdateApplyTaskCompleted(
        IsolatedWebAppUpdateApplyTask* task_ptr,
        IsolatedWebAppUpdateApplyTask::CompletionStatus status);

    base::raw_ref<IsolatedWebAppUpdateManager> update_manager_;

    // Update discovery tasks are executed serially one after each other. Only
    // the task at the front of the queue can be running. Once finished, the
    // task will be popped from the queue.
    base::circular_deque<std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask>>
        update_discovery_tasks_;
    base::Value::List update_discovery_results_log_;

    // Update apply tasks are executed serially one after each other. Only the
    // task at the front of the queue can be running. Once finished, the task
    // will be popped from the queue.
    base::circular_deque<std::unique_ptr<IsolatedWebAppUpdateApplyTask>>
        update_apply_tasks_;
    base::Value::List update_apply_results_log_;
  };

  // IwaKeyDistributionInfoProvider::Observer:
  void OnComponentUpdateSuccess(
      const base::Version& component_version) override;

  bool IsAnyIwaInstalled();

  // Queues new update discovery tasks and returns the number of new tasks that
  // have been queued.
  size_t QueueUpdateDiscoveryTasks();

  // Tries to queue an update discovery task for the provided `web_app`. It
  // might fail if the Update Manifest URL cannot be determined or if the app is
  // not an Isolated Web App.
  bool MaybeQueueUpdateDiscoveryTask(
      const WebApp& web_app,
      const base::flat_map<web_package::SignedWebBundleId,
                           IsolatedWebAppUpdateOptions>&
          id_to_update_options_map);

  void MaybeScheduleUpdateDiscoveryCheck();
  void MaybeResetScheduledUpdateDiscoveryCheck();

  void CreateUpdateApplyWaiter(
      const IsolatedWebAppUrlInfo& url_info,
      base::OnceClosure on_update_apply_task_created = base::DoNothing());

  void PrioritizeUpdateAndWaitImpl(
      const webapps::AppId& app_id,
      base::OnceCallback<void(IsolatedWebAppUpdateApplyTask::CompletionStatus)>
          callback);

  void OnUpdateDiscoveryTaskCompleted(
      std::unique_ptr<IsolatedWebAppUpdateDiscoveryTask> task,
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status);

  void OnUpdateApplyWaiterFinished(
      IsolatedWebAppUrlInfo url_info,
      base::OnceClosure on_update_apply_task_created,
      std::unique_ptr<ScopedKeepAlive> keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive);

  void OnUpdateApplyTaskCompleted(
      std::unique_ptr<IsolatedWebAppUpdateApplyTask> task,
      IsolatedWebAppUpdateApplyTask::CompletionStatus status);

  void OnLocalUpdateDiscovered(
      IsolatedWebAppUrlInfo url_info,
      base::OnceCallback<void(base::expected<base::Version, std::string>)>
          callback,
      base::expected<base::Version, std::string> update_discovery_result);

  void OnLocalUpdateApplyTaskCreated(
      IsolatedWebAppUrlInfo url_info,
      base::Version update_version,
      base::OnceCallback<void(base::expected<base::Version, std::string>)>
          callback);

  raw_ref<Profile> profile_;
  bool automatic_updates_enabled_;

  raw_ptr<WebAppProvider> provider_ = nullptr;

  bool has_started_ = false;

  class NextUpdateDiscoveryCheck {
   public:
    NextUpdateDiscoveryCheck();
    ~NextUpdateDiscoveryCheck();

    void ScheduleWithJitter(const base::TimeDelta& base_delay,
                            base::OnceClosure callback);

    std::optional<base::TimeTicks> GetScheduledTime() const;
    bool IsScheduled() const;
    void Reset();

    base::Value AsDebugValue() const;

   private:
    std::optional<std::pair<base::TimeTicks,
                            std::unique_ptr<base::CancelableOnceClosure>>>
        next_check_;
  };

  base::TimeDelta update_discovery_frequency_;
  NextUpdateDiscoveryCheck next_update_discovery_check_;

  TaskQueue task_queue_;

  base::flat_map<webapps::AppId,
                 std::unique_ptr<IsolatedWebAppUpdateApplyWaiter>>
      update_apply_waiters_;

  // Callbacks that are run once an update apply task for a given app id has
  // finished (successfully or unsuccessfully).
  base::flat_map<webapps::AppId,
                 std::unique_ptr<base::OnceCallbackList<void(
                     IsolatedWebAppUpdateApplyTask::CompletionStatus)>>>
      on_update_finished_callbacks_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::ScopedObservation<IwaKeyDistributionInfoProvider,
                          IwaKeyDistributionInfoProvider::Observer>
      key_distribution_info_observation_{this};

  class LocalDevModeUpdateDiscoverer;
  std::unique_ptr<LocalDevModeUpdateDiscoverer>
      local_dev_mode_update_discoverer_;

  base::ObserverList<Observer> task_observers_;

  base::WeakPtrFactory<IsolatedWebAppUpdateManager> weak_factory_{this};

  IsolatedWebAppUpdateError FromDiscoveryTaskError(
      const IsolatedWebAppUpdateDiscoveryTask::Error& error) const;

  void TrackResultOfUpdateDiscoveryTask(
      IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) const;

  void TrackResultOfUpdateApplyTask(
      IsolatedWebAppUpdateApplyTask::CompletionStatus status) const;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_MANAGER_H_
