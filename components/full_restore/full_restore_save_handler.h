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
#include "base/timer/timer.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace full_restore {

struct AppLaunchInfo;
class FullRestoreFileHandler;
class RestoreData;

// FullRestoreSaveHandler is responsible for writing both the app launch
// information and the app window information to disk. FullRestoreSaveHandler
// runs on the main thread and creates FullRestoreFileHandler (which runs on a
// background task runner) for the actual writing. To minimize IO,
// FullRestoreSaveHandler starts a timer that invokes restore data saving at a
// later time.
class COMPONENT_EXPORT(FULL_RESTORE) FullRestoreSaveHandler {
 public:
  static FullRestoreSaveHandler* GetInstance();

  FullRestoreSaveHandler();
  virtual ~FullRestoreSaveHandler();

  FullRestoreSaveHandler(const FullRestoreSaveHandler&) = delete;
  FullRestoreSaveHandler& operator=(const FullRestoreSaveHandler&) = delete;

  // Save |app_launch_info| to the full restore file in |profile_dir|.
  void SaveAppLaunchInfo(const base::FilePath& profile_dir,
                         std::unique_ptr<AppLaunchInfo> app_launch_info);

 private:
  // Starts the timer that invokes Save (if timer isn't already running).
  void MaybeStartSaveTimer();

  // Passes |file_path_to_file_handler_| to the backend for saving.
  void Save();

  // Invoked when write to file operation for |file_path| is finished.
  void OnSaveFinished(const base::FilePath& file_path);

  FullRestoreFileHandler* GetFileHandler(const base::FilePath& file_path);

  base::SequencedTaskRunner* BackendTaskRunner(const base::FilePath& file_path);

  // Records whether there are new updates for saving between each saving delay.
  // |ShouldUpdate| is cleared when Save is invoked.
  std::set<base::FilePath> should_update_;

  // The restore data for each user's profile. The key is the profile path.
  std::map<base::FilePath, RestoreData> file_path_to_restore_data_;

  // The file handler for each user's profile to write the restore data to the
  // full restore file for each user. The key is the profile path.
  std::map<base::FilePath, scoped_refptr<FullRestoreFileHandler>>
      file_path_to_file_handler_;

  // Timer used to delay the restore data writing to the full restore file.
  base::OneShotTimer save_timer_;

  // Records whether the saving process is running for a full restore file.
  std::set<base::FilePath> save_running_;

  base::WeakPtrFactory<FullRestoreSaveHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_FULL_RESTORE_SAVE_HANDLER_H_
