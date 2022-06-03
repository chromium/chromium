// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/process_singleton_lock_posix.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

const char kProcessSingletonLockDelimiter = '-';

bool ParseProcessSingletonLock(const base::FilePath& path,
                               std::string* hostname,
                               int* pid) {
  base::FilePath target;
  if (!base::ReadSymbolicLink(path, &target)) {
    // The only errno that should occur is ENOENT.
    if (errno != 0 && errno != ENOENT)
      PLOG(ERROR) << "readlink(" << path.value() << ") failed";
  }

  std::string real_path = target.value();
  if (real_path.empty())
    return false;

  std::string::size_type pos = real_path.rfind(kProcessSingletonLockDelimiter);

  // If the path is not a symbolic link, or doesn't contain what we expect,
  // bail.
  if (pos == std::string::npos) {
    *hostname = "";
    *pid = -1;
    return true;
  }

  *hostname = real_path.substr(0, pos);

  const std::string& pid_str = real_path.substr(pos + 1);
  if (!base::StringToInt(pid_str, pid))
    *pid = -1;

  return true;
}
