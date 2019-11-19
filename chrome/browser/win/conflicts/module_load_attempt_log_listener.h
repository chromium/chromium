// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_LOAD_ATTEMPT_LOG_LISTENER_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_LOAD_ATTEMPT_LOG_LISTENER_H_

#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_types.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"

namespace base {
class SequencedTaskRunner;
}

// This class drains the log of module load attempt from Chrome ELF, and
// notifies its delegate for all modules that were blocked.
class ModuleLoadAttemptLogListener : public base::win::ObjectWatcher::Delegate {
 public:
  using OnModuleBlockedCallback =
      base::RepeatingCallback<void(const base::FilePath& module_path,
                                   uint32_t module_size,
                                   uint32_t module_time_date_stamp)>;

  explicit ModuleLoadAttemptLogListener(
      OnModuleBlockedCallback on_module_blocked_callback);
  ~ModuleLoadAttemptLogListener() override;

  // base::win::ObjectWatcher::Delegate:
  void OnObjectSignaled(HANDLE object) override;

 private:
  void StartDrainingLogs();

  void OnLogDrained(
      std::vector<std::tuple<base::FilePath, uint32_t, uint32_t>>&&
          blocked_modules);

  // Translates a path of the form "\Device\HarddiskVolumeXX\..." into its
  // equivalent that starts with a drive letter ("C:\..."). Returns false on
  // failure.
  bool GetDriveLetterPath(const base::FilePath& device_path,
                          base::FilePath* drive_letter_path);

  // Update the |device_to_letter_path_mapping_|.
  void UpdateDeviceToLetterPathMapping();

  // Invoked once per blocked module every time the log is drained.
  OnModuleBlockedCallback on_module_blocked_callback_;

  // The sequence in which the log is drained.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Must be before |object_watcher_|.
  base::WaitableEvent waitable_event_;

  // Watches |waitable_event_|.
  base::win::ObjectWatcher object_watcher_;

  // A cache of the mapping of device path roots to their drive letter root
  // equivalent.
  std::vector<std::pair<base::FilePath, base::string16>>
      device_to_letter_path_mapping_;

  base::WeakPtrFactory<ModuleLoadAttemptLogListener> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ModuleLoadAttemptLogListener);
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_LOAD_ATTEMPT_LOG_LISTENER_H_
