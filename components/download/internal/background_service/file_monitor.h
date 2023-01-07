// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_FILE_MONITOR_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_FILE_MONITOR_H_

#include <memory>
#include <set>
#include <vector>

#include "components/download/internal/background_service/model.h"
#include "components/download/internal/background_service/stats.h"

namespace base {
class FilePath;
}  // namespace base

namespace download {

struct DriverEntry;

// An utility class containing various file cleanup methods.
class FileMonitor {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;

  // Creates the file directory for the downloads if it doesn't exist.
  virtual void Initialize(InitCallback callback) = 0;

  // Deletes the files in storage directory that are not related to any entries
  // in either database.
  virtual void DeleteUnknownFiles(
      const Model::EntryList& known_entries,
      const std::vector<DriverEntry>& known_driver_entries,
      base::OnceClosure completion_callback) = 0;

  // Deletes the files associated with the |entries|.
  virtual void CleanupFilesForCompletedEntries(
      const Model::EntryList& entries,
      base::OnceClosure completion_callback) = 0;

  // Deletes a list of files and logs UMA.
  virtual void DeleteFiles(const std::set<base::FilePath>& files_to_remove,
                           stats::FileCleanupReason reason) = 0;

  // Deletes all files in the download service directory.  This is a hard reset
  // on this directory.
  virtual void HardRecover(InitCallback callback) = 0;

  virtual ~FileMonitor() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_FILE_MONITOR_H_
