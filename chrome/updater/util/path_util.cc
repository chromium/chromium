// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/path_util.h"

#include "base/files/file_path.h"

namespace updater {

std::optional<base::FilePath> GetLogFilePath(UpdaterScope scope) {
  const std::optional<base::FilePath> log_dir = GetInstallDirectory(scope);
  if (log_dir) {
    return log_dir->Append(FILE_PATH_LITERAL("updater.log"));
  }
  return std::nullopt;
}

std::optional<base::FilePath> GetHistoryLogFilePath(UpdaterScope scope) {
  return GetInstallDirectory(scope).transform([](const base::FilePath& path) {
    return path.Append(FILE_PATH_LITERAL("updater_history.jsonl"));
  });
}

}  // namespace updater
