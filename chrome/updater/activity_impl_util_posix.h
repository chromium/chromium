// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_ACTIVITY_IMPL_UTIL_POSIX_H_
#define CHROME_UPDATER_ACTIVITY_IMPL_UTIL_POSIX_H_

#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace updater {
enum class UpdaterScope;

std::vector<base::FilePath> GetHomeDirPaths(UpdaterScope scope);

base::FilePath GetActiveFile(const base::FilePath& home_dir,
                             const std::string& id);

}  // namespace updater

#endif  // CHROME_UPDATER_ACTIVITY_IMPL_UTIL_POSIX_H_
