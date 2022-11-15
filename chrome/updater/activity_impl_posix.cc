// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity_impl.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "chrome/updater/activity_impl_util_posix.h"

namespace updater {
enum class UpdaterScope;

void ClearActiveBit(const base::FilePath& home_dir, const std::string& id) {
  struct stat home_buffer = {0};
  if (stat(home_dir.value().c_str(), &home_buffer)) {
    VPLOG(2) << "Failed to stat " << home_dir;
    return;
  }

  const base::FilePath active_file_path = GetActiveFile(home_dir, id);
  const base::ScopedFD dir_fd(
      HANDLE_EINTR(open(active_file_path.DirName().value().c_str(), O_RDONLY)));
  if (!dir_fd.is_valid()) {
    return;
  }
  struct stat active_file_buffer = {0};
  if (fstatat(dir_fd.get(), active_file_path.BaseName().value().c_str(),
              &active_file_buffer, AT_SYMLINK_NOFOLLOW)) {
    return;
  }
  if (active_file_buffer.st_uid != home_buffer.st_uid) {
    return;
  }
  unlinkat(dir_fd.get(), active_file_path.BaseName().value().c_str(), 0);
}

bool GetActiveBit(UpdaterScope scope, const std::string& id) {
  return base::ranges::any_of(
      GetHomeDirPaths(scope), [&id](const base::FilePath& path) {
        const base::FilePath& active_file = GetActiveFile(path, id);
        return base::PathExists(active_file) &&
               base::PathIsWritable(active_file);
      });
}

void ClearActiveBit(UpdaterScope scope, const std::string& id) {
  for (const base::FilePath& home_dir : GetHomeDirPaths(scope)) {
    ClearActiveBit(home_dir, id);
  }
}

}  // namespace updater
