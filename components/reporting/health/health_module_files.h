// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_FILES_H_
#define COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_FILES_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// HealthModuleFiles is a module used by the health module to persistently
// store ERP Health data. It contains the logic for reading and writing data to
// files and populating the ERPHealthData proto. Accesses to the underlying
// files should be called on the same sequenced task runner to achieve mutual
// exclusion.
class HealthModuleFiles {
 public:
  ~HealthModuleFiles();

  HealthModuleFiles(const HealthModuleFiles& other) = delete;
  HealthModuleFiles& operator=(const HealthModuleFiles& other) = delete;

  // Creation logic for the files interface. Maps what files are available
  // to be read, and the storage usage. Also attempts to clear up space if too
  // much is being used relative to |max_storage_space|.
  static std::unique_ptr<HealthModuleFiles> Create(
      const base::FilePath& directory,
      std::string_view file_base_name,
      uint32_t max_storage_space);

  // Dumps contents of underlying files into |data|.
  void PopulateHistory(ERPHealthData* data) const;

  // Writes data to the underlying files. If space is full, this will cause
  // other bits of data to be removed from the files.
  Status Write(std::string_view data);

 private:
  // Constructor for the class. Called by Create method.
  HealthModuleFiles(const base::FilePath& directory,
                    std::string_view file_base_name,
                    uint32_t max_storage_space,
                    uint32_t storage_used,
                    uint32_t max_file_header,
                    const std::map<uint32_t, base::FilePath>& files);

  // Frees |storage| space from underlying files. This will do nothing if
  // |storage| + |storage_used_| < |max_storage_space|. It may free more space
  // than |storage| as it will remove an entire line from the underlying files
  // (i.e. the oldest file's first line is 5 chars, and we request to remove 3
  // bytes. In this case the remaining 2 bytes of the line plus the newline
  // will also be removed.
  Status FreeStorage(uint32_t storage);

  // Requests a certain amount of storage from the health module. This will
  // trigger some data to be deleted from the underlying files if |storage| +
  // |storage_used_| > |max_storage_space|.
  Status ReserveStorage(uint32_t storage);

  // Add a new file to |files|.
  base::FilePath CreateNewFile();

  // Remove the last file in |files|. Age is tracked by the file header.
  void DeleteOldestFile();

  // Helper method checking size for a given file.
  static StatusOr<uint32_t> FileSize(const base::FilePath& file_path);

  SEQUENCE_CHECKER(sequence_checker_);

  // Root directory of ERP Health data files.
  const base::FilePath directory_;

  // Base name of files for the storage represented.
  const std::string file_base_name_;

  // Max storage space that can be used by every file.
  const uint32_t max_storage_space_;

  // Available storage space.
  uint32_t storage_used_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Current largest file header in use.
  uint32_t max_file_header_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  // Files currently managed for a given history.
  std::map<uint32_t, base::FilePath> files_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Max storage used by an individual history, currently constant 10KiB.
  // TODO(tylergarrett) control each history per policy.
  const uint32_t max_file_storage_ = 10 * 1024u;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_HEALTH_HEALTH_MODULE_FILES_H_
