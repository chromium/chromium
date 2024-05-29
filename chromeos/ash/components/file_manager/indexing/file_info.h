// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INFO_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INFO_H_

#include <optional>
#include <ostream>
#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash::file_manager {

// Represents information about the file. Both local and remote (cloud based)
// files can be represented by this structure.
struct COMPONENT_EXPORT(FILE_MANAGER) FileInfo {
  // Creates a file info object with the specified values.
  FileInfo(const GURL& file_url, int64_t size, base::Time last_modified);
  ~FileInfo();

  // TODO(b:327535200): Reconsider copyability.
  FileInfo(const FileInfo& file_info);
  FileInfo& operator=(const FileInfo& file_info);

  // Returns whether this file info is less than `other`.
  // This method uses just the `file_url`.
  bool operator<(const FileInfo& other) const {
    return file_url < other.file_url;
  }

  // Returns whether this file info is equal to `other`. For two files to
  // be equal all fields must be equal.
  bool operator==(const FileInfo& other) const;

  // Human readable output of this structure.
  friend COMPONENT_EXPORT(FILE_MANAGER) std::ostream& operator<<(
      std::ostream& stream, const FileInfo& file_info);

  // The URL of the file in the form filesystem:<origin>:<type>:/path/to/entry.
  // For example, for a file stored on the local file system the URL would be
  // filesystem:chrome://file-manager/external/Downloads-123..xy/foo.txt
  // For a file from Google Drive the URL would be
  // filesystem:chrome://file-manager/drivefs-123..xy/root/bar.txt
  GURL file_url;

  // The size of the file if known, or 0 otherwise.
  int64_t size;

  // The modification time of the file.
  base::Time last_modified;

  // A remote ID; used only for files located in the cloud, otherwise empty.
  std::optional<std::string> remote_id;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INFO_H_
