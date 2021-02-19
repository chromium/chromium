// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_FULL_RESTORE_READ_HANDLER_H_
#define COMPONENTS_FULL_RESTORE_FULL_RESTORE_READ_HANDLER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace full_restore {

class FullRestoreFileHandler;
class RestoreData;
struct WindowInfo;

// FullRestoreSaveHandler is responsible for reading |RestoreData| from the full
// restore data file. RestoreHandler runs on the main thread and creates
// FullRestoreFileHandler (which runs on a background task runner) for the
// actual reading.
class COMPONENT_EXPORT(FULL_RESTORE) FullRestoreReadHandler
    : public aura::EnvObserver,
      public aura::WindowObserver {
 public:
  // The callback function to get the restore data when the reading operation is
  // done.
  using Callback = base::OnceCallback<void(std::unique_ptr<RestoreData>)>;

  static FullRestoreReadHandler* GetInstance();

  FullRestoreReadHandler();
  FullRestoreReadHandler(const FullRestoreReadHandler&) = delete;
  FullRestoreReadHandler& operator=(const FullRestoreReadHandler&) = delete;
  ~FullRestoreReadHandler() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  void SetActiveProfilePath(const base::FilePath& profile_path);

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

  // Gets the window information for |restore_window_id| or |window|.
  std::unique_ptr<WindowInfo> GetWindowInfo(int32_t restore_window_id);
  std::unique_ptr<WindowInfo> GetWindowInfo(aura::Window* window);

  // Fetches the restore id for the window from RestoreData for the given
  // |app_id|. |app_id| should be a Chrome app id.
  int32_t FetchRestoreWindowId(const std::string& app_id);

 private:
  // Invoked when reading the restore data from |profile_path| is finished, and
  // calls |callback| to notify that the reading operation is done.
  void OnGetRestoreData(const base::FilePath& profile_path,
                        Callback callback,
                        std::unique_ptr<RestoreData>);

  // Removes AppRestoreData for |restore_window_id|.
  void RemoveAppRestoreData(int restore_window_id);

  // The current active user profile path.
  base::FilePath active_profile_path_;

  // The restore data read from the full restore files.
  std::map<base::FilePath, std::unique_ptr<RestoreData>>
      profile_path_to_restore_data_;

  // The map from the window id to the full restore file path and the
  // app id. The window id id is saved in the window property
  // |kRestoreWindowIdKey|. This map is used to find the file path and the app
  // id when get the window info.
  std::map<int32_t, std::pair<base::FilePath, std::string>>
      window_id_to_app_restore_info_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  base::WeakPtrFactory<FullRestoreReadHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_READ_HANDLER_H_
