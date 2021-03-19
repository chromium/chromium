// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity_impl.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "chrome/updater/mac/mac_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
namespace {

base::FilePath GetActiveFile(const base::FilePath& home_dir,
                             const std::string& id) {
  return home_dir.AppendASCII("Library")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

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

}  // namespace

bool GetActiveBit(UpdaterScope scope, const std::string& id) {
  // TODO(crbug.com/1096654): Add support for UpdaterScope::kSystem.
  const base::FilePath path = GetActiveFile(base::GetHomeDir(), id);
  return base::PathExists(path) && base::PathIsWritable(path);
}

void ClearActiveBit(UpdaterScope scope, const std::string& id) {
  // TODO(crbug.com/1096654): Add support for UpdaterScope::kSystem.
  ClearActiveBit(base::GetHomeDir(), id);
}

}  // namespace updater
