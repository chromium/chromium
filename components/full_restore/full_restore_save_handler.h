// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_

#include <map>
#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace full_restore {

struct AppLaunchInfo;
class FullRestoreFileHandler;
class RestoreData;
struct WindowInfo;

// FullRestoreSaveHandler is responsible for writing both the app launch
// information and the app window information to disk. FullRestoreSaveHandler
// runs on the main thread and creates FullRestoreFileHandler (which runs on a
// background task runner) for the actual writing. To minimize IO,
// FullRestoreSaveHandler starts a timer that invokes restore data saving at a
// later time.
class COMPONENT_EXPORT(FULL_RESTORE) FullRestoreSaveHandler
    : public aura::EnvObserver,
      public aura::WindowObserver {
 public:
  static FullRestoreSaveHandler* GetInstance();

  FullRestoreSaveHandler();
  ~FullRestoreSaveHandler() override;

  FullRestoreSaveHandler(const FullRestoreSaveHandler&) = delete;
  FullRestoreSaveHandler& operator=(const FullRestoreSaveHandler&) = delete;

  void SetActiveProfilePath(const base::FilePath& profile_path);

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // Saves |app_launch_info| to the full restore file in |profile_path|.
  void SaveAppLaunchInfo(const base::FilePath& profile_path,
                         std::unique_ptr<AppLaunchInfo> app_launch_info);

  // Saves |window_info| to |profile_path_to_restore_data_|.
  void SaveWindowInfo(const WindowInfo& window_info);

  // Flushes the full restore file in |profile_path| with the current restore
  // data.
  void Flush(const base::FilePath& profile_path);

  // Removes app launching and app windows for an app with the given |app_id|
  // from |file_path_to_restore_data_| for |profile_path| .
  void RemoveApp(const base::FilePath& profile_path, const std::string& app_id);

  base::OneShotTimer* GetTimerForTesting() { return &save_timer_; }

 private:
  // Starts the timer that invokes Save (if timer isn't already running).
  void MaybeStartSaveTimer();

  // Passes |profile_path_to_restore_data_| to the backend for saving.
  void Save();

  // Invoked when write to file operation for |profile_path| is finished.
  void OnSaveFinished(const base::FilePath& profile_path);

  FullRestoreFileHandler* GetFileHandler(const base::FilePath& profile_path);

  base::SequencedTaskRunner* BackendTaskRunner(
      const base::FilePath& profile_path);

  // Add |app_launch_info| to |app_id_to_launch_list_|.
  void AddAppLaunchInfo(const base::FilePath& profile_path,
                        std::unique_ptr<AppLaunchInfo> app_launch_info);

  // Removes AppRestoreData for |window_id|.
  void RemoveAppRestoreData(int window_id);

  // Records whether there are new updates for saving between each saving delay.
  // |pending_save_profile_paths_| is cleared when Save is invoked.
  std::set<base::FilePath> pending_save_profile_paths_;

  // The restore data for each user's profile. The key is the profile path.
  std::map<base::FilePath, RestoreData> profile_path_to_restore_data_;

  // The file handler for each user's profile to write the restore data to the
  // full restore file for each user. The key is the profile path.
  std::map<base::FilePath, scoped_refptr<FullRestoreFileHandler>>
      profile_path_to_file_handler_;

  // The map from the window id to the full restore file path and the app id.
  // The window id is saved in the window property. This map is used to find the
  // file path and the app id when save the window info.
  std::map<int32_t, std::pair<base::FilePath, std::string>>
      window_id_to_app_restore_info_;

  // The map from the app id to the full restore file path and the app launch
  // info.
  std::map<std::string,
           std::pair<base::FilePath, std::unique_ptr<AppLaunchInfo>>>
      app_id_to_app_launch_info_;

  // The current active user profile path.
  base::FilePath active_profile_path_;

  // Timer used to delay the restore data writing to the full restore file.
  base::OneShotTimer save_timer_;

  // Records whether the saving process is running for a full restore file.
  std::set<base::FilePath> save_running_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  base::WeakPtrFactory<FullRestoreSaveHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
