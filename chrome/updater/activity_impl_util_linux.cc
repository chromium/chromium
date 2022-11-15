// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity_impl_util_posix.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
std::vector<base::FilePath> GetHomeDirPaths(UpdaterScope scope) {
  NOTREACHED();
  return {};
}

base::FilePath GetActiveFile(const base::FilePath& home_dir,
                             const std::string& id) {
  NOTREACHED();
  return base::FilePath();
}

}  // namespace updater
