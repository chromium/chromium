// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_MTAB_WATCHER_LINUX_H_
#define COMPONENTS_STORAGE_MONITOR_MTAB_WATCHER_LINUX_H_

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#error "ChromeOS does not use MtabWatcherLinux."
#endif

#include <map>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"

namespace storage_monitor {

// MtabWatcherLinux listens for mount point changes from a mtab file and
// notifies a StorageMonitorLinux about them. This class should be created and
// destroyed on a single sequence suitable for file IO.
class MtabWatcherLinux {
 public:
  // (mount point, mount device)
  // A mapping from mount point to mount device, as extracted from the mtab
  // file.
  using MountPointDeviceMap = std::map<base::FilePath, base::FilePath>;

  using UpdateMtabCallback =
      base::RepeatingCallback<void(const MountPointDeviceMap& new_mtab)>;

  // |callback| is called on the same sequence as the rest of the class.
  // Caller is responsible for bouncing to the correct sequence.
  MtabWatcherLinux(const base::FilePath& mtab_path,
                   const UpdateMtabCallback& callback);

  MtabWatcherLinux(const MtabWatcherLinux&) = delete;
  MtabWatcherLinux& operator=(const MtabWatcherLinux&) = delete;

  ~MtabWatcherLinux();

 private:
  // Reads mtab file entries into |mtab|.
  void ReadMtab() const;

  // Called when |mtab_path_| changes.
  void OnFilePathChanged(const base::FilePath& path, bool error);

  // Mtab file that lists the mount points.
  const base::FilePath mtab_path_;

  // Watcher for |mtab_path_|.
  base::FilePathWatcher file_watcher_;

  UpdateMtabCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MtabWatcherLinux> weak_ptr_factory_{this};
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MTAB_WATCHER_LINUX_H_
