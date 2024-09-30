// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
#define COMPONENTS_APP_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "components/app_restore/app_restore_arc_info.h"
#include "components/app_restore/arc_save_handler.h"
#include "components/app_restore/lacros_save_handler.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace apps {
class AppRegistryCache;
}  // namespace apps

namespace app_restore {
struct AppLaunchInfo;
class RestoreData;
struct WindowInfo;
}  // namespace app_restore

namespace ash::full_restore {
class FullRestoreServiceTestHavingFullRestoreFile;
class FullRestoreAppLaunchHandlerArcAppBrowserTest;
}  // namespace ash::full_restore

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace full_restore {

class FullRestoreFileHandler;

// FullRestoreSaveHandler is responsible for writing both the app launch
// information and the app window information to disk. FullRestoreSaveHandler
// runs on the main thread and creates FullRestoreFileHandler (which runs on a
// background task runner) for the actual writing. To minimize IO,
// FullRestoreSaveHandler starts a timer that invokes restore data saving at a
// later time.
class COMPONENT_EXPORT(APP_RESTORE) FullRestoreSaveHandler
    : public aura::EnvObserver,
      public aura::WindowObserver,
      public app_restore::AppRestoreArcInfo::Observer {
 public:
  using AppLaunchInfoPtr = std::unique_ptr<app_restore::AppLaunchInfo>;

  static FullRestoreSaveHandler* GetInstance();

  FullRestoreSaveHandler();
  FullRestoreSaveHandler(const FullRestoreSaveHandler&) = delete;
  FullRestoreSaveHandler& operator=(const FullRestoreSaveHandler&) = delete;
  ~FullRestoreSaveHandler() override;

  void InsertIgnoreApplicationId(const std::string& app_id);

  void SetPrimaryProfilePath(const base::FilePath& profile_path);

  void SetActiveProfilePath(const base::FilePath& profile_path);

  void SetAppRegistryCache(const base::FilePath& profile_path,
                           apps::AppRegistryCache* app_registry_cache);

  // When called, allows the Save() method to write to disk. Schedules the save
  // timer to start for each monitored profile.
  void AllowSave();

  void SetShutDown();

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // app_restore::AppRestoreArcInfo::Observer:
  void OnTaskCreated(const std::string& app_id,
                     int32_t task_id,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnArcConnectionChanged(bool is_connection_ready) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnTaskThemeColorUpdated(int32_t task_id,
                               uint32_t primary_color,
                               uint32_t status_bar_color) override;

  // Saves |app_launch_info| to the full restore file in |profile_path|.
  void SaveAppLaunchInfo(
      const base::FilePath& profile_path,
      std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info);

  // Saves |window_info| to |profile_path_to_restore_data_|.
  void SaveWindowInfo(const app_restore::WindowInfo& window_info);

  // Saves `removing_desk_guid` to the restore data for the currently active
  // profile path.
  void SaveRemovingDeskGuid(const base::Uuid& removing_desk_guid);

  // Invoked when an Chrome app Lacros window is created. `app_id` is the
  // AppService id, and `window_id` is the wayland app_id property for the
  // window.
  void OnLacrosChromeAppWindowAdded(const std::string& app_id,
                                    const std::string& window_id);

  // Invoked when an Chrome app Lacros window is removed. `app_id` is the
  // AppService id, and `window_id` is the wayland app_id property for the
  // window.
  void OnLacrosChromeAppWindowRemoved(const std::string& app_id,
                                      const std::string& window_id);

  // Flushes the full restore file in |profile_path| with the current restore
  // data.
  void Flush(const base::FilePath& profile_path);

  // Returns true if there is a AppRestoreData for the given `profile_path`,
  // `app_id` and `window_id`. Otherwise, returns false.
  bool HasAppRestoreData(const base::FilePath& profile_path,
                         const std::string& app_id,
                         int32_t window_id);

  // Saves |app_launch_info| to |profile_path_to_file_handler_| for
  // |profile_path| which will be written to the full restore file, if
  // |app_launch_info| has a window_id.
  void AddAppLaunchInfo(const base::FilePath& profile_path,
                        AppLaunchInfoPtr app_launch_info);

  // Modify the window id for `app_id` from `old_window_id` to `new_window_id`.
  void ModifyWindowId(const base::FilePath& profile_path,
                      const std::string& app_id,
                      int32_t old_window_id,
                      int32_t new_window_id);

  // Saves |window_info| to |profile_path| for |app_id| and |window_id|.
  void ModifyWindowInfo(const base::FilePath& profile_path,
                        const std::string& app_id,
                        int32_t window_id,
                        const app_restore::WindowInfo& window_info);

  // Saves |primary_color| and |status_bar_color| to |profile_path| for |app_id|
  // and |window_id|.
  void ModifyThemeColor(const base::FilePath& profile_path,
                        const std::string& app_id,
                        int32_t window_id,
                        uint32_t primary_color,
                        uint32_t status_bar_color);

  // Removes app launching and app windows for an app with the given |app_id|
  // from |file_path_to_restore_data_| for |profile_path| .
  void RemoveApp(const base::FilePath& profile_path, const std::string& app_id);

  // Removes AppRestoreData from |profile_path| for |app_id| and |window_id|.
  void RemoveAppRestoreData(const base::FilePath& profile_path,
                            const std::string& app_id,
                            int window_id);

  // Sends the window for `profile_path` `app_id and `window_id` to background.
  void SendWindowToBackground(const base::FilePath& profile_path,
                              const std::string& app_id,
                              int window_id);

  // Starts the timer, and when timeout, clears restore data for |profile_path|.
  void ClearRestoreData(const base::FilePath& profile_path);

  // Generates the ARC session id (0 - 1,000,000,000) for ARC apps.
  int32_t GetArcSessionId();

  // Returns the RestoreData that associates with |profile_path|. Returns
  // nullptr if there is no such RestoreData.
  const app_restore::RestoreData* GetRestoreData(
      const base::FilePath& profile_path);

  // Returns the full restore app id for |window| that can be used to look up
  // the window's associated AppRestoreData.
  std::string GetAppId(aura::Window* window);

  // Returns the window id of a chrome app hosted in lacros. Returns -1 if
  // `window` is not in the lacros save handler.
  int GetLacrosChromeAppWindowId(aura::Window* window) const;

  // Fetches the app launch information from `app_id_to_app_launch_infos_` for
  // the given `profile_path` and `app_id`. `app_id` should be a Chrome app id.
  AppLaunchInfoPtr FetchAppLaunchInfo(const base::FilePath& profile_path,
                                      const std::string& app_id);

  // Returns the window information from the restore data of `profile_path` for
  // `app_id` and `window_id`.
  std::unique_ptr<app_restore::WindowInfo> GetWindowInfo(
      const base::FilePath& profile_path,
      const std::string& app_id,
      int window_id);

  base::OneShotTimer* GetTimerForTesting() { return &save_timer_; }

  // Since this is a singleton, tests may need to clear it between tests.
  void ClearForTesting();

 private:
  friend class FullRestoreSaveHandlerTestApi;
  friend class ash::full_restore::FullRestoreServiceTestHavingFullRestoreFile;
  friend class ash::full_restore::FullRestoreAppLaunchHandlerArcAppBrowserTest;

  // Map from a profile path to AppLaunchInfos.
  using AppLaunchInfos =
      base::flat_map<base::FilePath, std::list<AppLaunchInfoPtr>>;

  // Starts the timer that invokes Save (if timer isn't already running).
  void MaybeStartSaveTimer(const base::FilePath& profile_path);

  // Passes |profile_path_to_restore_data_| to the backend for saving.
  void Save();

  // Invoked when write to file operation for |profile_path| is finished.
  void OnSaveFinished(const base::FilePath& profile_path);

  FullRestoreFileHandler* GetFileHandler(const base::FilePath& profile_path);

  base::SequencedTaskRunner* BackendTaskRunner(
      const base::FilePath& profile_path);

  // Saves |window_info| to |profile_path_to_file_handler_|.
  void ModifyWindowInfo(int window_id,
                        const app_restore::WindowInfo& window_info);

  // Removes AppRestoreData for |window_id|.
  void RemoveAppRestoreData(int window_id);

  // Applications with their app ids in this set will not have their app launch
  // infos saved.
  base::flat_set<std::string> ignore_applications_ids_;

  // FullRestoreSaveHandler might be called to save the help app before
  // FullRestoreAppLaunchHandler reads the full restore data from the full
  // restore file during the system startup phase, e.g. when a new user login.
  // So call FullRestoreReadHandler to read the file before saving the new data.
  // `been_read_profile_paths_` is used to save the profile paths, whose full
  // restore file has been read by FullRestoreReadHandler.
  std::set<base::FilePath> been_read_profile_paths_;

  // Records whether there are new updates for saving between each saving delay.
  // |pending_save_profile_paths_| is cleared when Save is invoked.
  std::set<base::FilePath> pending_save_profile_paths_;

  // The restore data for each user's profile. The key is the profile path.
  std::map<base::FilePath, app_restore::RestoreData>
      profile_path_to_restore_data_;

  // The file handler for each user's profile to write the restore data to the
  // full restore file for each user. The key is the profile path.
  base::flat_map<base::FilePath, scoped_refptr<FullRestoreFileHandler>>
      profile_path_to_file_handler_;

  // The AppRegistryCache for each user's profile. The key is the profile path.
  base::flat_map<base::FilePath,
                 raw_ptr<apps::AppRegistryCache, CtnExperimental>>
      profile_path_to_app_registry_cache_;

  // The map from the window id to the full restore file path and the app id.
  // The window id is saved in the window property. This map is used to find the
  // file path and the app id for browser windows and Chrome app windows only
  // when save the window info. This map can't be used for ARC app windows.
  std::map<int32_t, std::pair<base::FilePath, std::string>>
      window_id_to_app_restore_info_;

  // The map from the app id to the app launch info for each full restore file
  // path.
  std::map<std::string, AppLaunchInfos> app_id_to_app_launch_infos_;

  // The current active user profile path.
  base::FilePath active_profile_path_;

  // Timer used to delay the restore data writing to the full restore file.
  base::OneShotTimer save_timer_;

  // During the startup phase, start `wait_timer_` to wait for the system
  // finishes the startup and the restore process, to prevent the original
  // restore data is overwritten if the system restarts due to fast crash or
  // upgrading.
  base::OneShotTimer wait_timer_;

  // Records whether the saving process is running for a full restore file.
  std::set<base::FilePath> save_running_;

  std::unique_ptr<ArcSaveHandler> arc_save_handler_;

  std::unique_ptr<LacrosSaveHandler> lacros_save_handler_;

  bool is_shut_down_ = false;

  // Due to the system crash or upgrading, the system might restart or reboot
  // very fast after startup. If the new window is written for the first time
  // startup, after the second time reboot, the original restore data can't be
  // restored. For the user, it looks like not restore. So block the save timer
  // when startup until one of the below condition is matched:
  // 1. restore finish if the restore setting is always, and no crash.
  // 2. restore finish if there is a restore notification, and the user selects
  // restore.
  // 3. an app is launched by the user if there is a restore notification.
  // 4. the restore notification is cancel or closed by the user if there is a
  // restore notification.
  // 5. the restore setting is off.
  // 6. 'wait_timer_' is expired.
  //
  // When one of the above condition is matched, allow_save_ is set as true to
  // permit `save_timer_` to start periodically triggering saving to disk.
  bool allow_save_ = false;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  base::ScopedObservation<app_restore::AppRestoreArcInfo,
                          app_restore::AppRestoreArcInfo::Observer>
      arc_info_observer_{this};

  base::WeakPtrFactory<FullRestoreSaveHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
