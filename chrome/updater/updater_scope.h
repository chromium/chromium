// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATER_SCOPE_H_
#define CHROME_UPDATER_UPDATER_SCOPE_H_

#include <ostream>
#include <string>

#include "base/command_line.h"

namespace updater {

// Scope of the service invocation.
enum class UpdaterScope {
  // The updater is running in the logged in user's scope.
  kUser = 1,

  // The updater is running in the system's scope.
  kSystem = 2,
};

inline std::string UpdaterScopeToString(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return "User";
    case UpdaterScope::kSystem:
      return "System";
  }
}

inline std::ostream& operator<<(std::ostream& os, UpdaterScope scope) {
  return os << UpdaterScopeToString(scope).c_str();
}

// Returns `true` if the tag has a "needsadmin=prefers" argument.
bool IsPrefersForCommandLine(const base::CommandLine& command_line);

// Returns the scope of the updater, which is either per-system or per-user.
// The updater scope is determined from the `command_line` argument.
UpdaterScope GetUpdaterScopeForCommandLine(
    const base::CommandLine& command_line);

// Returns the scope of the updater, which is either per-system or per-user.
// The updater scope is determined from command line arguments of the process,
// the presence and content of the tag, and the integrity level of the process,
// where applicable.
UpdaterScope GetUpdaterScope();

bool IsSystemInstall();
bool IsSystemInstall(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATER_SCOPE_H_
