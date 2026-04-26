// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/installer/util/duplicate_tree_detector.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace installer {

bool IsIdenticalFileHierarchy(const base::FilePath& src_path,
                              const base::FilePath& dest_path) {
  base::File::Info src_info;
  if (!base::GetFileInfo(src_path, &src_info)) {
    PLOG(ERROR) << "Failed to get file info for source path: " << src_path;
    return false;
  }

  base::File::Info dest_info;
  if (!base::GetFileInfo(dest_path, &dest_info)) {
    PLOG(ERROR) << "Failed to get file info for destination path: "
                << dest_path;
    return false;
  }

  if (src_info.is_directory != dest_info.is_directory) {
    // The two paths are of different types, so they cannot be identical.
    VLOG(1) << "Path types differ: " << src_path << " vs " << dest_path;
    return false;
  }

  if (!src_info.is_directory) {
    // Two files are "identical" if the file sizes are equivalent.
    if (src_info.size != dest_info.size) {
      VLOG(1) << "Files differ in size: " << src_path << "(" << src_info.size
              << ") vs " << dest_path << "(" << dest_info.size << ")";
      return false;
    }
#if !defined(OFFICIAL_BUILD)
    // For developer builds, also check last modification time (to make sure
    // version dir DLLs are replaced on over-install even if the tested change
    // doesn't happen to change a given DLL's size).
    if (src_info.last_modified != dest_info.last_modified) {
      VLOG(1) << "Files differ in modification time: " << src_path << "("
              << src_info.last_modified << ")  vs " << dest_path << "("
              << dest_info.last_modified << ")";
      return false;
    }
#endif
    // The files are identical.
    return true;
  }

  // Two directories are "identical" if dest_path contains entries that are
  // "identical" to all the entries in src_path.
  base::FileEnumerator path_enum(
      src_path, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      /*pattern=*/{}, base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (base::FilePath path = path_enum.Next(); !path.empty();
       path = path_enum.Next()) {
    if (!IsIdenticalFileHierarchy(path, dest_path.Append(path.BaseName()))) {
      VLOG(1) << "Sub-hierarchy differs: " << path << " vs "
              << dest_path.Append(path.BaseName());
      return false;
    }
  }
  if (auto error = path_enum.GetError(); error != base::File::FILE_OK) {
    PLOG(ERROR) << "Failed to enumerate files in directory: " << src_path
                << " with error: " << base::File::ErrorToString(error);
    return false;
  }
  // The directories are identical.
  return true;
}

}  // namespace installer
