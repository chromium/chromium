// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/updater/activity_impl_util_posix.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

std::vector<base::FilePath> GetHomeDirPaths(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser: {
      const base::FilePath path = base::GetHomeDir();
      if (path.empty()) {
        return {};
      }
      return {path};
    }
    case UpdaterScope::kSystem: {
      std::vector<base::FilePath> home_dir_paths;
      base::FileEnumerator(base::FilePath(FILE_PATH_LITERAL("/Users")),
                           /*recursive=*/false,
                           base::FileEnumerator::DIRECTORIES)
          .ForEach([&home_dir_paths](const base::FilePath& name) {
            if (base::PathIsWritable(name)) {
              home_dir_paths.push_back(name);
            }
          });
      return home_dir_paths;
    }
  }
}

base::FilePath GetActiveFile(const base::FilePath& home_dir,
                             const std::string& id) {
  return home_dir.AppendASCII("Library")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

}  // namespace updater
