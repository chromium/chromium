// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_EMPTY_FILE_MONITOR_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_EMPTY_FILE_MONITOR_H_

#include "components/download/internal/background_service/file_monitor.h"

#include "base/memory/weak_ptr.h"

namespace download {

// File monitor implementation that does nothing, used in incognito mode without
// any file persisted to disk.
class EmptyFileMonitor : public FileMonitor {
 public:
  EmptyFileMonitor();

  EmptyFileMonitor(const EmptyFileMonitor&) = delete;
  EmptyFileMonitor& operator=(const EmptyFileMonitor&) = delete;

  ~EmptyFileMonitor() override;

 private:
  // FileMonitor implementation.
  void Initialize(InitCallback callback) override;
  void DeleteUnknownFiles(const Model::EntryList& known_entries,
                          const std::vector<DriverEntry>& known_driver_entries,
                          base::OnceClosure completion_callback) override;
  void CleanupFilesForCompletedEntries(
      const Model::EntryList& entries,
      base::OnceClosure completion_callback) override;
  void DeleteFiles(const std::set<base::FilePath>& files_to_remove,
                   stats::FileCleanupReason reason) override;
  void HardRecover(InitCallback callback) override;

  base::WeakPtrFactory<EmptyFileMonitor> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_EMPTY_FILE_MONITOR_H_
