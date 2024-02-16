// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_FULL_RESTORE_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_FULL_RESTORE_READ_HANDLER_H_

#include <memory>
#include <utility>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/app_restore/app_restore_arc_info.h"
#include "components/app_restore/arc_read_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace app_restore {
struct AppLaunchInfo;
class RestoreData;
struct WindowInfo;
}  // namespace app_restore

namespace ash {
class AppLaunchInfoSaveWaiter;
namespace full_restore {
class FullRestoreAppLaunchHandlerTestBase;
class FullRestoreServiceTestHavingFullRestoreFile;
}  // namespace full_restore
}  // namespace ash

namespace full_restore {

class FullRestoreFileHandler;

// FullRestoreReadHandler is responsible for reading |RestoreData| from the full
// restore data file. FullRestoreReadHandler runs on the main thread and creates
// FullRestoreFileHandler (which runs on a background task runner) for the
// actual reading.
class COMPONENT_EXPORT(APP_RESTORE) FullRestoreReadHandler
    : public aura::EnvObserver,
      public aura::WindowObserver,
      public app_restore::ArcReadHandler::Delegate,
      public app_restore::AppRestoreArcInfo::Observer {
 public:
  // The callback function to get the restore data when the reading operation is
  // done.
  using Callback =
      base::OnceCallback<void(std::unique_ptr<app_restore::RestoreData>)>;

  static FullRestoreReadHandler* GetInstance();

  FullRestoreReadHandler();
  FullRestoreReadHandler(const FullRestoreReadHandler&) = delete;
  FullRestoreReadHandler& operator=(const FullRestoreReadHandler&) = delete;
  ~FullRestoreReadHandler() override;

  app_restore::ArcReadHandler* arc_read_handler() {
    return arc_read_handler_.get();
  }

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // app_restore::ArcReadHandler::Delegate:
  std::unique_ptr<app_restore::AppLaunchInfo> GetAppLaunchInfo(
      const base::FilePath& profile_path,
      const std::string& app_id,
      int32_t restore_window_id) override;
  std::unique_ptr<app_restore::WindowInfo> GetWindowInfo(
      const base::FilePath& profile_path,
      const std::string& app_id,
      int32_t restore_window_id) override;
  void RemoveAppRestoreData(const base::FilePath& profile_path,
                            const std::string& app_id,
                            int32_t restore_window_id) override;

  // app_restore::AppRestoreArcInfo::Observer:
  void OnTaskCreated(const std::string& app_id,
                     int32_t task_id,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;

  void SetPrimaryProfilePath(const base::FilePath& profile_path);

  void SetActiveProfilePath(const base::FilePath& profile_path);

  // Sets whether we should check the restore data for `profile_path`. If the
  // user selects `Restore`, then we should check the restore data for restored
  // windows. Otherwise, we don't need to to check the restore data.
  void SetCheckRestoreData(const base::FilePath& profile_path);

  // Reads the restore data from |profile_path| on a background task runner, and
  // calls |callback| when the reading operation is done.
  void ReadFromFile(const base::FilePath& profile_path, Callback callback);

  // Modifies the restore data from |profile_path| to set the next restore
  // window id for the given chrome app |app_id|.
  void SetNextRestoreWindowIdForChromeApp(const base::FilePath& profile_path,
                                          const std::string& app_id);

  // Removes app launching and app windows for an app with the given |app_id|
  // from |profile_path_to_restore_data_| for |profile_path| .
  void RemoveApp(const base::FilePath& profile_path, const std::string& app_id);

  // Returns true if there are app type browsers from the full restore file.
  // Otherwise, returns false.
  bool HasAppTypeBrowser(const base::FilePath& profile_path);

  // Returns true if there are normal browsers (non app type browser) from the
  // full restore file. Otherwise, returns false.
  bool HasBrowser(const base::FilePath& profile_path);

  // Returns true if there is a window info for |restore_window_id| from the
  // full restore file. Otherwise, returns false. This interface can't be used
  // for Arc app windows.
  bool HasWindowInfo(int32_t restore_window_id);

  // Gets the window information for |window|.
  std::unique_ptr<app_restore::WindowInfo> GetWindowInfo(aura::Window* window);

  // Gets the window information for the active profile associated with
  // `restore_window_id`.
  std::unique_ptr<app_restore::WindowInfo> GetWindowInfoForActiveProfile(
      int32_t restore_window_id);

  // Gets the ARC app launch information from the full restore file for `app_id`
  // and `session_id`.
  std::unique_ptr<app_restore::AppLaunchInfo> GetArcAppLaunchInfo(
      const std::string& app_id,
      int32_t session_id);

  // Fetches the restore id for the window from RestoreData for the given
  // |app_id|. |app_id| should be a Chrome app id.
  int32_t FetchRestoreWindowId(const std::string& app_id);

  // Returns the restore window id for the ARC app's |task_id|.
  int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id);

  // Returns the restore window id for the ARC app's |session_id|.
  int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id);

  // Returns the restore window id for the Lacros window with
  // `lacros_window_id`.
  int32_t GetLacrosRestoreWindowId(const std::string& lacros_window_id) const;

  // Sets |arc session id| for |window_id| to |arc_session_id_to_window_id_|.
  // |arc session id| is assigned when ARC apps are restored.
  void SetArcSessionIdForWindowId(int32_t arc_session_id, int32_t window_id);

  // Called when full restore launching is about to begin. Saves the start time
  // in `profile_path_to_start_time_data_`.
  void SetStartTimeForProfile(const base::FilePath& profile_path);

  // Returns true if full restore launching is thought to be underway on
  // `active_profile_path_`.
  bool IsFullRestoreRunning() const;

  void AddChromeBrowserLaunchInfoForTesting(const base::FilePath& profile_path);

 private:
  friend class ash::AppLaunchInfoSaveWaiter;
  friend class ash::full_restore::FullRestoreAppLaunchHandlerTestBase;
  friend class ash::full_restore::FullRestoreServiceTestHavingFullRestoreFile;
  friend class FullRestoreReadHandlerTestApi;

  // Gets the window information for |restore_window_id| for browser windows and
  // Chrome app windows only. This interface can't be used for ARC app windows.
  std::unique_ptr<app_restore::WindowInfo> GetWindowInfo(
      int32_t restore_window_id);

  // Returns true if ARC restore launching is thought to be underway on
  // `primary_profile_path_`.
  bool IsArcRestoreRunning() const;

  // Returns true if Lacros restore launching is thought to be underway on
  // `primary_profile_path_`.
  bool IsLacrosRestoreRunning() const;

  // Invoked when reading the restore data from |profile_path| is finished, and
  // calls |callback| to notify that the reading operation is done.
  void OnGetRestoreData(const base::FilePath& profile_path,
                        Callback callback,
                        std::unique_ptr<app_restore::RestoreData>);

  // Removes AppRestoreData for |restore_window_id|.
  void RemoveAppRestoreData(int32_t restore_window_id);

  app_restore::RestoreData* GetRestoreData(const base::FilePath& profile_path);

  // The primary user profile path.
  base::FilePath primary_profile_path_;

  // The current active user profile path.
  base::FilePath active_profile_path_;

  // The restore data read from the full restore files.
  base::flat_map<base::FilePath, std::unique_ptr<app_restore::RestoreData>>
      profile_path_to_restore_data_;

  // The map from the window id to the full restore file path and the
  // app id. The window id is saved in the window property
  // |kRestoreWindowIdKey|. This map is used to find the file path and the app
  // id when get the window info. This map is not used for ARC app windows.
  base::flat_map<int32_t, std::pair<base::FilePath, std::string>>
      window_id_to_app_restore_info_;

  // The start time of full restore for each profile. There won't be an entry if
  // full restore hasn't started for the profile.
  base::flat_map<base::FilePath, base::TimeTicks>
      profile_path_to_start_time_data_;

  std::unique_ptr<app_restore::ArcReadHandler> arc_read_handler_;

  // Records whether we need to check the restore data for the profile path. If
  // the profile path is recorded, we should check the restore data. Otherwise,
  // we don't need to check the restore data, because the restore process hasn't
  // started yet.
  std::set<base::FilePath> should_check_restore_data_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  base::ScopedObservation<app_restore::AppRestoreArcInfo,
                          app_restore::AppRestoreArcInfo::Observer>
      arc_info_observer_{this};

  base::WeakPtrFactory<FullRestoreReadHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_FULL_RESTORE_READ_HANDLER_H_
