// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_PATH_UTIL_H_
#define CHROME_UPDATER_UTIL_PATH_UTIL_H_

#include <optional>

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_path_util.h"  // IWYU pragma: export
#endif

namespace base {
class FilePath;
}

namespace updater {

enum class UpdaterScope;

// Returns the base install directory common to all versions of the updater.
// Does not create the directory if it does not exist.
std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope);

std::optional<base::FilePath> GetLogFilePath(UpdaterScope scope);

// Returns the path to the history JSONL file for an updater installation. A
// path is returned regardless of if the file exists. This method does not
// perform any IO; it may be called from any sequence.
std::optional<base::FilePath> GetHistoryLogFilePath(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_PATH_UTIL_H_
