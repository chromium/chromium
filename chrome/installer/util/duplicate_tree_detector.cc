// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/installer/util/duplicate_tree_detector.h"

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"

namespace installer {

bool IsIdenticalFileHierarchy(const base::FilePath& src_path,
                              const base::FilePath& dest_path) {
  base::File::Info src_info;
  base::File::Info dest_info;

  bool is_identical = false;
  if (base::GetFileInfo(src_path, &src_info) &&
      base::GetFileInfo(dest_path, &dest_info)) {
    // Both paths exist, check the types:
    if (!src_info.is_directory && !dest_info.is_directory) {
      // Two files are "identical" if the file sizes are equivalent.
      is_identical = src_info.size == dest_info.size;
#if !defined(OFFICIAL_BUILD)
      // For developer builds, also check last modification time (to make sure
      // version dir DLLs are replaced on over-install even if the tested change
      // doesn't happen to change a given DLL's size).
      if (is_identical)
        is_identical = (src_info.last_modified == dest_info.last_modified);
#endif
    } else if (src_info.is_directory && dest_info.is_directory) {
      // Two directories are "identical" if dest_path contains entries that are
      // "identical" to all the entries in src_path.
      is_identical = true;

      base::FileEnumerator path_enum(
          src_path, false /* not recursive */,
          base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
      for (base::FilePath path = path_enum.Next();
           is_identical && !path.empty(); path = path_enum.Next()) {
        is_identical =
            IsIdenticalFileHierarchy(path, dest_path.Append(path.BaseName()));
      }
    } else {
      // The two paths are of different types, so they cannot be identical.
      DCHECK(!is_identical);
    }
  }

  return is_identical;
}

}  // namespace installer
