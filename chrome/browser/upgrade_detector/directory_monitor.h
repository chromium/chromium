// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_DIRECTORY_MONITOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_DIRECTORY_MONITOR_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"

namespace base {
class FilePathWatcher;
class SequencedTaskRunner;
}  // namespace base

// A monitor of installs that watches for changes in the browser's installation
// directory.
class DirectoryMonitor final : public InstalledVersionMonitor {
 public:
  explicit DirectoryMonitor(base::FilePath install_dir);
  ~DirectoryMonitor() override;

  // InstalledVersionMonitor:
  void Start(Callback on_change_callback) override;

 private:
  base::FilePath install_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_DIRECTORY_MONITOR_H_
