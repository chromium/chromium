// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/base_file.h"

#include <errno.h>

#include "base/files/file_util.h"
#include "components/download/public/common/download_interrupt_reasons.h"

namespace download {

DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const base::FilePath& new_path) {
  // Similarly, on Unix, we're moving a temp file created with permissions 600
  // to |new_path|. Here, we try to fix up the destination file with appropriate
  // permissions.
  struct stat st;
  // First check the file existence and create an empty file if it doesn't
  // exist.
  if (!base::PathExists(new_path)) {
    if (!base::WriteFile(new_path, "")) {
      return LogSystemError("WriteFile", errno);
    }
  }
  int stat_error = stat(new_path.value().c_str(), &st);
  bool stat_succeeded = (stat_error == 0);
  if (!stat_succeeded)
    LogSystemError("stat", errno);

  if (!base::Move(full_path_, new_path))
    return LogSystemError("Move", errno);

  if (stat_succeeded) {
    // On Windows file systems (FAT, NTFS), chmod fails.  This is OK.
    int chmod_error = chmod(new_path.value().c_str(), st.st_mode);
    if (chmod_error < 0)
      LogSystemError("chmod", errno);
  }
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

}  // namespace download
