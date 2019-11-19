// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_EMPTY_FILE_MONITOR_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_EMPTY_FILE_MONITOR_H_

#include "components/download/internal/background_service/file_monitor.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace download {

// File monitor implementation that does nothing, used in incognito mode without
// any file persisted to disk.
class EmptyFileMonitor : public FileMonitor {
 public:
  EmptyFileMonitor();
  ~EmptyFileMonitor() override;

 private:
  // FileMonitor implementation.
  void Initialize(const InitCallback& callback) override;
  void DeleteUnknownFiles(
      const Model::EntryList& known_entries,
      const std::vector<DriverEntry>& known_driver_entries) override;
  void CleanupFilesForCompletedEntries(
      const Model::EntryList& entries,
      const base::RepeatingClosure& completion_callback) override;
  void DeleteFiles(const std::set<base::FilePath>& files_to_remove,
                   stats::FileCleanupReason reason) override;
  void HardRecover(const InitCallback& callback) override;

  base::WeakPtrFactory<EmptyFileMonitor> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EmptyFileMonitor);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_EMPTY_FILE_MONITOR_H_
