// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "chrome/updater/activity_impl_util_posix.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
namespace {
// Attempts to discover all of the home directories on the system by parsing
// /etc/passwd.
std::vector<base::FilePath> ReadHomeDirsFromPasswd() {
  std::string passwd_contents;
  if (!base::ReadFileToString(base::FilePath("/etc/passwd"),
                              &passwd_contents)) {
    return {};
  }

  // /etc/passwd contains one line for each user account, with seven
  // fields delimited by colons.
  std::vector<base::FilePath> home_dirs;
  base::ranges::transform(
      base::SplitString(passwd_contents, "\n",
                        base::WhitespaceHandling::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY),
      std::back_inserter(home_dirs), [](const std::string& line) {
        std::vector<std::string> entries = base::SplitString(
            line, ":", base::WhitespaceHandling::KEEP_WHITESPACE,
            base::SplitResult::SPLIT_WANT_ALL);
        // The sixth entry (index 5) of the line will be the user's home
        // directory.
        return entries.size() == 7 ? base::FilePath(entries[5])
                                   : base::FilePath();
      });
  // Remove invalid paths
  base::ranges::remove(home_dirs, base::FilePath());
  return home_dirs;
}
}  // namespace

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
      return ReadHomeDirsFromPasswd();
    }
  }
  return {};
}

base::FilePath GetActiveFile(const base::FilePath& home_dir,
                             const std::string& id) {
  return home_dir.AppendASCII(".local")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING)
      .AppendASCII("Actives")
      .AppendASCII(id);
}
}  // namespace updater
