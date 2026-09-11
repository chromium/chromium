// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_file_utils.h"

#include "base/files/file.h"
#include "base/files/file_util.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace download {

bool ReplaceFileWithPermissions(const base::FilePath& from_path,
                                const base::FilePath& to_path) {
#if BUILDFLAG(IS_POSIX)
  // Create an empty file none exists at `to_path` to ensure that the file
  // permissions are preserved when moving the file.
  if (!base::PathExists(to_path)) {
    if (!base::WriteFile(to_path, "")) {
      return false;
    }
  }

  struct stat st;
  int stat_error = base::File::Stat(to_path, &st);
  bool stat_succeeded = (stat_error == 0);
  if (!base::Move(from_path, to_path)) {
    return false;
  }

  if (stat_succeeded) {
    // On Windows file systems (FAT, NTFS), chmod fails. This is OK.
    chmod(to_path.value().c_str(), st.st_mode);
  }
  return true;
#else
  // On Windows, base::ReplaceFile preserves ACLs and file attributes. On
  // Fuchsia, file permissions are not supported, so a simple atomic rename
  // via base::ReplaceFile is sufficient.
  return base::ReplaceFile(from_path, to_path, nullptr);
#endif
}

bool WriteFileAtomicallyWithPermissions(const base::FilePath& path,
                                        base::span<const uint8_t> data) {
  base::FilePath temp_path;
  base::File temp_file =
      base::CreateAndOpenTemporaryFileInDir(path.DirName(), &temp_path);
  if (!temp_file.IsValid()) {
    return false;
  }

  if (!temp_file.WriteAndCheck(0, data)) {
    temp_file.Close();
    base::DeleteFile(temp_path);
    return false;
  }
  temp_file.Close();

  if (!ReplaceFileWithPermissions(temp_path, path)) {
    base::DeleteFile(temp_path);
    return false;
  }

  return true;
}

}  // namespace download
